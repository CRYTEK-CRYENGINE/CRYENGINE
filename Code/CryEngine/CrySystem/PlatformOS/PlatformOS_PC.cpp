// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PlatformOS_PC.cpp
//  Created:     11/02/2010 by Alex McCarthy.
//  Description: Implementation of the IPlatformOS interface for PC
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include <StdAfx.h>

#if !CRY_PLATFORM_DURANGO

	#include "PlatformOS_PC.h"
	#include "SaveReaderWriter_CryPak.h"
	#include <CrySystem/File/ICryPak.h>

	#include <CryNetwork/CrySocks.h>

	#include "../ZipDir.h"
	#include "../CryArchive.h"
	#include <CryNetwork/INetwork.h>
	#include <CrySystem/ZLib/IZLibCompressor.h>
	#include <CryString/StringUtils.h>

#if CRY_PLATFORM_WINDOWS
	#include <timeapi.h>
#endif

IPlatformOS* IPlatformOS::Create(const uint8 createParams)
{
	return new CPlatformOS_PC(createParams);
}

CPlatformOS_PC::CPlatformOS_PC(const uint8 createParams)
	: m_listeners(4)
	, m_fpsWatcher(15.0f, 3.0f, 7.0f)
	, m_delayLevelStartIcon(0.0f)
	, m_bSignedIn(false)
	, m_bSaving(false)
	, m_bLevelLoad(false)
	, m_bSaveDuringLevelLoad(false)
{
	AddListener(this, "PC");

	gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CPlatformOS_PC_SystemEventListener");

	m_cachePakStatus = 0;
	m_cachePakUser = -1;
	//TODO: Handle early corruption detection (createParams & IPlatformOS::eCF_EarlyCorruptionDetected ) if necessary.
}

CPlatformOS_PC::~CPlatformOS_PC()
{
	gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
}

void CPlatformOS_PC::Tick(float realFrameTime)
{
	if (m_delayLevelStartIcon)
	{
		m_delayLevelStartIcon -= realFrameTime;
		if (m_delayLevelStartIcon <= 0.0f)
		{
			m_delayLevelStartIcon = 0.0f;

			IPlatformOS::SPlatformEvent event(0);
			event.m_eEventType = IPlatformOS::SPlatformEvent::eET_FileWrite;
			event.m_uParams.m_fileWrite.m_type = SPlatformEvent::eFWT_SaveStart;
			NotifyListeners(event);

			event.m_eEventType = IPlatformOS::SPlatformEvent::eET_FileWrite;
			event.m_uParams.m_fileWrite.m_type = SPlatformEvent::eFWT_SaveEnd;
			NotifyListeners(event);
		}
	}

	SaveDirtyFiles();
}

void CPlatformOS_PC::OnPlatformEvent(const IPlatformOS::SPlatformEvent& _event)
{
	switch (_event.m_eEventType)
	{
	case SPlatformEvent::eET_FileWrite:
		{
			if (_event.m_uParams.m_fileWrite.m_type == SPlatformEvent::eFWT_CheckpointLevelStart)
			{
				m_delayLevelStartIcon = 5.0f;
			}
			break;
		}
	}
}

bool CPlatformOS_PC::GetUserProfilePreference(unsigned int user, EUserProfilePreference ePreference, SUserProfileVariant& outResult) const
{
	return false;
}

unsigned int CPlatformOS_PC::UserGetMaximumSignedInUsers() const
{
	return 1;
}

bool CPlatformOS_PC::UserIsSignedIn(unsigned int userIndex) const
{
	return userIndex != Unknown_User && m_bSignedIn;
}

bool CPlatformOS_PC::UserDoSignIn(unsigned int numUsersRequested, unsigned int /*controllerIndex*/)
{
	if (!m_bSignedIn)
	{
		m_bSignedIn = true;

		// Tell the system that we are signed in
		IPlatformOS::SPlatformEvent event(0);
		event.m_eEventType = SPlatformEvent::eET_SignIn;
		event.m_uParams.m_signIn.m_previousSignedInState = SPlatformEvent::eSIS_NotSignedIn;
		event.m_uParams.m_signIn.m_signedInState = IPlatformOS::SPlatformEvent::eSIS_SignedInLocally; // TODO: we may need to pass live
		NotifyListeners(event);

		// Tell the system that storage is mounted - required for CGame
		event.m_eEventType = SPlatformEvent::eET_StorageMounted;
		event.m_uParams.m_storageMounted.m_bPhysicalMedia = true;
		event.m_uParams.m_storageMounted.m_bOnlyUpdateMediaState = false;
		NotifyListeners(event);
	}
	return true;
}

bool CPlatformOS_PC::UserGetName(unsigned int userIndex, IPlatformOS::TUserName& outName) const
{
	#if CRY_PLATFORM_WINDOWS
	DWORD numChars = outName.MAX_SIZE;
	std::vector<wchar_t> nameW(numChars);
	BOOL e = GetUserNameW(&nameW[0], &numChars);
	nameW[numChars] = 0;
	// UNICODE to UTF-8 for correct file system operation
	CryStringUtils::WStrToUTF8(wstring(&nameW[0]), outName);
	return m_bSignedIn && (e ? true : false);
	#else
	outName.assign(gEnv->pSystem->GetUserName());
	return true;
	#endif
}

bool CPlatformOS_PC::UserGetOnlineName(unsigned int userIndex, IPlatformOS::TUserName& outName) const
{
	// No distinction between offline and online names on pc at this time.
	return UserGetName(userIndex, outName);
}

// func returns true if the data is correctly encrypted and signed. caller is then responsible for calling delete[] on the returned data buffer
// returns false if there is a problem, caller has no data to free if false is returned
bool CPlatformOS_PC::DecryptAndCheckSigning(const char* pInData, int inDataLen, char** pOutData, int* pOutDataLen, const uint8 key[16])
{
	INetwork* pNet = GetISystem()->GetINetwork();
	IZLibCompressor* pZLib = GetISystem()->GetIZLibCompressor();
	TCipher cipher = pNet->BeginCipher(key, 8);         // Use the first 8 bytes of the key as the decryption key
	char* pOutput = NULL;
	int outDataLen = 0;
	bool valid = false;

	if (inDataLen > 16)
	{
		if (cipher)
		{
			pOutput = new char[inDataLen];
			if (pOutput)
			{
				pNet->Decrypt(cipher, (uint8*)pOutput, (const uint8*)pInData, inDataLen);

				// validate cksum to check for successful decryption and for data signing
				SMD5Context ctx;
				char digest[16];

				pZLib->MD5Init(&ctx);
				pZLib->MD5Update(&ctx, (const char*)(key + 8), 8); // Use the last 8 bytes of the key as the signing key
				pZLib->MD5Update(&ctx, pOutput, inDataLen - 16);
				pZLib->MD5Final(&ctx, digest);

				if (memcmp(digest, pOutput + inDataLen - 16, 16) != 0)
				{
					CryLog("MD5 fail on dlc xml");
				}
				else
				{
					CryLog("dlc xml passed decrypt and MD5 checks");
					valid = true;
				}
			}
			pNet->EndCipher(cipher);
		}
	}

	if (valid)
	{
		*pOutData = pOutput;
		*pOutDataLen = inDataLen - 16;
	}
	else
	{
		delete[] pOutput;
		*pOutData = NULL;
		*pOutDataLen = 0;
	}

	return valid;
}

void CPlatformOS_PC::MountDLCContent(IDLCListener* pCallback, unsigned int user, const uint8 keyData[16])
{
	//Get the DLC install directory
	const char* dlcDir = "$DLC$";
	int nDLCPacksFound = 0;

	ICryPak* pCryPak = gEnv->pCryPak;

	if (pCryPak->GetAlias(dlcDir) == NULL)
	{
		pCallback->OnDLCMountFailed(eDMF_NoDLCDir);
	}
	else
	{

		CryFixedStringT<ICryPak::g_nMaxPath> findPath;
		findPath.Format("%s/*", dlcDir);

		CryLog("Passing %s to File Finder (dlcDir %s", findPath.c_str(), dlcDir);

		// Search for all subfolders with a file named dlc.sxml, this corresponds to a DLC package
		IFileFinderPtr fileFinder = GetFileFinder(user);
		_finddata_t fd;
		intptr_t nFS = fileFinder->FindFirst(findPath.c_str(), &fd);
		if (nFS != -1)
		{
			do
			{
				// Skip files, only want subdirectories
				if (!(fd.attrib & _A_SUBDIR) || !strcmp(fd.name, ".") || !strcmp(fd.name, ".."))
				{
					continue;
				}

				// Try to load the xml file
				stack_string path;
				path.Format("%s/%s/dlc.sxml", dlcDir, fd.name);

				CryLog("DLC: Trying %s", path.c_str());

				FILE* pFile = pCryPak->FOpen(path.c_str(), "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FOPEN_ONDISK | ICryArchive::FLAGS_ABSOLUTE_PATHS);
				if (pFile)
				{
					CryLog("DLC: Found %s", path.c_str());
					bool success = false;
					pCryPak->FSeek(pFile, 0, SEEK_END);
					int size = pCryPak->FTell(pFile);
					pCryPak->FSeek(pFile, 0, SEEK_SET);
					char* pData = new char[size];
					pCryPak->FRead(pData, size, pFile);
					pCryPak->FClose(pFile);

					char* pDecryptedData = NULL;
					int decryptedSize = 0;
					if (DecryptAndCheckSigning(pData, size, &pDecryptedData, &decryptedSize, keyData))
					{
						XmlNodeRef rootNode = gEnv->pSystem->LoadXmlFromBuffer(pDecryptedData, decryptedSize);
						if (rootNode)
						{
							path.Format("%s/%s", dlcDir, fd.name);
							pCallback->OnDLCMounted(rootNode, path.c_str());
							success = true;

							nDLCPacksFound++;
						}
						delete[] pDecryptedData;
					}
					delete[] pData;
					if (!success)
					{
						pCallback->OnDLCMountFailed(eDMF_XmlError);
					}
				}
			}
			while (0 == fileFinder->FindNext(nFS, &fd));
			fileFinder->FindClose(nFS);
		}
	}

	pCallback->OnDLCMountFinished(nDLCPacksFound);
}

bool CPlatformOS_PC::CanRestartTitle() const
{
	return false;
}

void CPlatformOS_PC::RestartTitle(const char* pTitle)
{
	CRY_ASSERT_MESSAGE(CanRestartTitle(), "Restart title not implemented (or previously needed)");
}

bool CPlatformOS_PC::UsePlatformSavingAPI() const
{
	// Default true if CVar doesn't exist
	ICVar* pUseAPI = gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_usePlatformSavingAPI") : NULL;
	return !pUseAPI || pUseAPI->GetIVal() != 0;
}

bool CPlatformOS_PC::BeginSaveLoad(unsigned int user, bool bSave)
{
	m_bSaving = bSave;
	return true;
}

void CPlatformOS_PC::EndSaveLoad(unsigned int user)
{
}

IPlatformOS::ISaveReaderPtr CPlatformOS_PC::SaveGetReader(const char* fileName, unsigned int /*user*/)
{
	CSaveReader_CryPakPtr pSaveReader(new CSaveReader_CryPak(fileName));

	if (!pSaveReader || pSaveReader->LastError() != IPlatformOS::eFOC_Success)
	{
		return CSaveReader_CryPakPtr();
	}
	else
	{
		return pSaveReader;
	}
}

IPlatformOS::ISaveWriterPtr CPlatformOS_PC::SaveGetWriter(const char* fileName, unsigned int /*user*/)
{
	CSaveWriter_CryPakPtr pSaveWriter(new CSaveWriter_CryPak(fileName));

	if (!pSaveWriter || pSaveWriter->LastError() != IPlatformOS::eFOC_Success)
	{
		return CSaveWriter_CryPakPtr();
	}
	else
	{
		if (m_bLevelLoad)
		{
			m_bSaveDuringLevelLoad = true;
		}

		return pSaveWriter;
	}
}

void CPlatformOS_PC::InitEncryptionKey(const char* pMagic, size_t magicLength, const uint8* pKey, size_t keyLength)
{
	m_encryptionMagic.resize(magicLength);
	memcpy(&m_encryptionMagic[0], pMagic, magicLength);
	m_encryptionKey.resize(keyLength);
	memcpy(&m_encryptionKey[0], pKey, keyLength);
}

void CPlatformOS_PC::GetEncryptionKey(const std::vector<char>** pMagic, const std::vector<uint8>** pKey)
{
	if (pMagic) *pMagic = &m_encryptionMagic;
	if (pKey) *pKey = &m_encryptionKey;
}

/*
   --------------------- Listener -----------------------
 */

void CPlatformOS_PC::AddListener(IPlatformOS::IPlatformListener* pListener, const char* szName)
{
	m_listeners.Add(pListener, szName);
}

bool CPlatformOS_PC::PostLocalizationBootChecks()
{
	//Not currently implemented
	return true;
}

void CPlatformOS_PC::SetOpticalDriveIdle(bool bIdle)
{
	//Not currently implemented
}
void CPlatformOS_PC::AllowOpticalDriveUsage(bool bAllow)
{
	//Not currently implemented
}

void CPlatformOS_PC::RemoveListener(IPlatformOS::IPlatformListener* pListener)
{
	m_listeners.Remove(pListener);
}

void CPlatformOS_PC::NotifyListeners(SPlatformEvent& event)
{
	for (CListenerSet<IPlatformOS::IPlatformListener*>::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
		notifier->OnPlatformEvent(event);
}

bool CPlatformOS_PC::KeyboardStart(unsigned int inUserIndex, unsigned int flags, const char* title, const char* initialInput, int maxInputLength, IVirtualKeyboardEvents* pInCallback)
{
	return false;
}

bool CPlatformOS_PC::KeyboardIsRunning()
{
	return false;
}

bool CPlatformOS_PC::KeyboardCancel()
{
	return false;
}

bool CPlatformOS_PC::StringVerifyStart(const char* inString, IStringVerifyEvents* pInCallback)
{
	return false;
}

bool CPlatformOS_PC::IsVerifyingString()
{
	return false;
}

ILocalizationManager::EPlatformIndependentLanguageID CPlatformOS_PC::GetSystemLanguage()
{
	//Not yet implemented
	return ILocalizationManager::ePILID_MAX_OR_INVALID;
}

const char* CPlatformOS_PC::GetSKUId()
{
	//Not yet implemented
	return NULL;
}

ILocalizationManager::TLocalizationBitfield CPlatformOS_PC::GetSystemSupportedLanguages()
{
	//Not yet implemented
	return 0;
}

void CPlatformOS_PC::GetMemoryUsage(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	m_listeners.GetMemoryUsage(pSizer);
}

bool CPlatformOS_PC::DebugSave(SDebugDump& dump)
{
	return false;
}

bool CPlatformOS_PC::ConsoleLoadGame(struct IConsoleCmdArgs* pArgs)
{
	return false;
}

int CPlatformOS_PC::GetFirstSignedInUser() const
{
	if (UserIsSignedIn(Unknown_User)) // default user
		return Unknown_User;

	unsigned int maximumSignedInUsers = UserGetMaximumSignedInUsers();
	for (unsigned int user = 0; user < maximumSignedInUsers; ++user)
	{
		if (UserIsSignedIn(user))
		{
			return static_cast<int>(user);
		}
	}

	return Unknown_User;
}

EUserPIIStatus CPlatformOS_PC::GetUserPII(unsigned int inUser, SUserPII* pOutPII)
{
	return k_pii_error;
}

const char* const CPlatformOS_PC::GetHostName()
{
	#define HOSTNAME_MAXLENGTH (256)
	static char s_hostnameBuffer[HOSTNAME_MAXLENGTH] = { 0 };
	if (s_hostnameBuffer[0] == 0)
	{
		unsigned long int maxLength = HOSTNAME_MAXLENGTH;
		gethostname(s_hostnameBuffer, maxLength);
		s_hostnameBuffer[maxLength - 1] = 0; // Ensure 0 termination
	}
	return s_hostnameBuffer;
}

IPlatformOS::EZipExtractFail CPlatformOS_PC::ExtractZips(const char* path)
{
	IPlatformOS::EZipExtractFail retVal = eZEF_Success;
	CryLog("DLCZip: Extracting Downloaded zips");

	//get the path to the DLC install directory

	char userPath[ICryPak::g_nMaxPath];
	gEnv->pCryPak->AdjustFileName(path, userPath, 0);

	//look for zips
	IPlatformOS::IFileFinderPtr fileFinder = GetFileFinder(0);
	_finddata_t fd;
	intptr_t nFS = fileFinder->FindFirst("%USER%/DLC/*", &fd);
	if (nFS != -1)
	{
		do
		{
			stack_string filePath;
			filePath.Format("%s/%s", userPath, fd.name);

			// Skip dirs, only want files, and want them to be zips
			if ((fd.attrib == _A_SUBDIR) || strstr(fd.name, ".zip") == 0)
			{
				continue;
			}

			ICryArchive* pArc = gEnv->pCryPak->OpenArchive(filePath.c_str());

			if (pArc != NULL)
			{

				//find the files in the archive...
				ZipDir::CacheRWPtr pZip = static_cast<CryArchiveRW*>(pArc)->GetCache();
				ZipDir::FileEntryTree* pZipRoot = pZip->GetRoot();

				if (SxmlMissingFromHDD(pZipRoot, userPath, pZip))
				{
					retVal = RecurseZipContents(pZipRoot, userPath, pZip);
				}

			}
		}
		while (0 == fileFinder->FindNext(nFS, &fd) && retVal == eZEF_Success);

		fileFinder->FindClose(nFS);
	}

	CryLog("DLCZip: Finished Extracting zips");

	return retVal;
}

IPlatformOS::EZipExtractFail CPlatformOS_PC::RecurseZipContents(ZipDir::FileEntryTree* pSourceDir, const char* currentPath, ZipDir::CacheRWPtr pCache)
{
	EZipExtractFail retVal = eZEF_Success;

	ZipDir::FileEntryTree::FileMap::iterator fileIt = pSourceDir->GetFileBegin();
	ZipDir::FileEntryTree::FileMap::iterator fileEndIt = pSourceDir->GetFileEnd();

	//look for files and output them to disk
	CryFixedStringT<ICryPak::g_nMaxPath> filePath;
	for (; fileIt != fileEndIt && retVal == eZEF_Success; ++fileIt)
	{
		ZipDir::FileEntry* pFileEntry = pSourceDir->GetFileEntry(fileIt);
		const char* pFileName = pSourceDir->GetFileName(fileIt);

		filePath.Format("%s/%s", currentPath, pFileName);

		void* pData = pCache->AllocAndReadFile(pFileEntry);

		FILE* pNewFile = gEnv->pCryPak->FOpen(filePath.c_str(), "wb");
		size_t written = gEnv->pCryPak->FWrite(pData, 1, pFileEntry->desc.lSizeUncompressed, pNewFile);

		if (pFileEntry->desc.lSizeUncompressed != written)
		{
			//failed to extract file from zip, report error
			//drop out as fast and cleanly as we can
			retVal = eZEF_WriteFail;
		}

		gEnv->pCryPak->FClose(pNewFile);

		pCache->Free(pData);
	}

	ZipDir::FileEntryTree::SubdirMap::iterator dirIt = pSourceDir->GetDirBegin();
	ZipDir::FileEntryTree::SubdirMap::iterator dirEndIt = pSourceDir->GetDirEnd();

	//look for deeper directories in the heirarchy
	CryFixedStringT<ICryPak::g_nMaxPath> dirPath;
	for (; dirIt != dirEndIt && retVal == eZEF_Success; ++dirIt)
	{
		dirPath.Format("%s/%s", currentPath, pSourceDir->GetDirName(dirIt));
		gEnv->pCryPak->MakeDir(dirPath);

		retVal = RecurseZipContents(pSourceDir->GetDirEntry(dirIt), dirPath.c_str(), pCache);
	}

	return retVal;
}

bool CPlatformOS_PC::SxmlMissingFromHDD(ZipDir::FileEntryTree* pZipRoot, const char* userPath, ZipDir::CacheRWPtr pZip)
{
	//return true if sxml is present in zip but not at equivalent path on HDD
	bool sxmlInZip = false;
	bool sxmlOnHDD = false;

	//sxml exists inside one of the dirs in the root of the zip

	ZipDir::FileEntryTree::SubdirMap::iterator dirIt = pZipRoot->GetDirBegin();
	ZipDir::FileEntryTree::SubdirMap::iterator dirEndIt = pZipRoot->GetDirEnd();
	CryFixedStringT<ICryPak::g_nMaxPath> dirPath;
	for (; dirIt != dirEndIt; ++dirIt)
	{
		dirPath.Format("%s/%s", userPath, pZipRoot->GetDirName(dirIt));
		//gEnv->pCryPak->MakeDir( dirPath );

		ZipDir::FileEntryTree* pSourceDir = pZipRoot->GetDirEntry(dirIt);

		ZipDir::FileEntryTree::FileMap::iterator fileIt = pSourceDir->GetFileBegin();
		ZipDir::FileEntryTree::FileMap::iterator fileEndIt = pSourceDir->GetFileEnd();

		//look for files
		CryFixedStringT<ICryPak::g_nMaxPath> filePath;
		for (; fileIt != fileEndIt; ++fileIt)
		{
			ZipDir::FileEntry* pFileEntry = pSourceDir->GetFileEntry(fileIt);
			const char* pFileName = pSourceDir->GetFileName(fileIt);

			if (strstr(pFileName, ".sxml"))
			{
				sxmlInZip = true;
				filePath.Format("%s/%s", dirPath.c_str(), pFileName);

				//this is the path where the sxml should be, is it present correctly?

				ICryPak* pCryPak = gEnv->pCryPak;
				FILE* pFile = pCryPak->FOpen(filePath.c_str(), "rb", ICryPak::FOPEN_HINT_QUIET | ICryPak::FOPEN_ONDISK | ICryArchive::FLAGS_ABSOLUTE_PATHS);
				if (pFile)
				{
					sxmlOnHDD = true;
				}
			}
		}
	}

	if (sxmlInZip == true && sxmlOnHDD == false)
	{
		return true;
	}
	else
	{
		//either zip doesn't have an sxml (it's not a dlc zip) or has already been correctly extracted to disk
		return false;
	}
}

void CPlatformOS_PC::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_LEVEL_LOAD_START:
		m_bLevelLoad = true;
		m_bSaveDuringLevelLoad = false;
		break;
	case ESYSTEM_EVENT_LEVEL_LOAD_END:
		m_bLevelLoad = false;
		m_bSaveDuringLevelLoad = true;
		break;
	case ESYSTEM_EVENT_CHANGE_FOCUS:
	{
#if CRY_PLATFORM_WINDOWS
		// Handle system timer resolution
		// The smaller the resolution, the more accurate a thread will wake up from a suspension 
		// Example: 
		// Timer Resolution(1 ms): Sleep(1) -> max thread suspention time 1.99ms
		// Timer Resolution(15 ms): Sleep(1) -> max thread suspention time 15.99ms
		//  
		// This is due to the scheduler running more frequently.
		// This is a system wide global though which is set to the smallest value requested by a process.
		// When the process dies it is set back to its original value.
		ICVar* pSystemTimerResolution = gEnv->pConsole ? gEnv->pConsole->GetCVar("sys_system_timer_resolution") : NULL;
		TIMECAPS tc;		
		if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) == TIMERR_NOERROR)
		{
			const UINT minTimerRes =  std::min(std::max((UINT)pSystemTimerResolution->GetIVal(), tc.wPeriodMin), tc.wPeriodMax);

			// wparam != 0 is focused, wparam == 0 is not focused
			timeBeginPeriod(wparam != 0 ? minTimerRes : tc.wPeriodMax);
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "Warning: System Timer Resolution could not be obtained.");
		}

		break;
#endif
	}
	}
}

void CPlatformOS_PC::OnActionEvent(const SActionEvent& event)
{
	switch (event.m_event)
	{
	case eAE_mapCmdIssued:
		m_bLevelLoad = true;
		m_bSaveDuringLevelLoad = false;
		break;

	case eAE_inGame:
		m_bLevelLoad = false;
		m_fpsWatcher.Reset();
		break;
	}
}

void CPlatformOS_PC::SaveDirtyFiles()
{
	if (m_bSaveDuringLevelLoad)
	{
		// Wait for level load to finish
		if (m_bLevelLoad)
		{
			m_fpsWatcher.Reset();
			return;
		}

		if (!m_fpsWatcher.HasAchievedStableFPS())
		{
			return;
		}

		m_bSaveDuringLevelLoad = false;
		m_bLevelLoad = false;
	}

	if (m_bSaving)
	{
		if (!m_delayLevelStartIcon)
		{
			IPlatformOS::SPlatformEvent event(0);
			event.m_eEventType = IPlatformOS::SPlatformEvent::eET_FileWrite;
			event.m_uParams.m_fileWrite.m_type = SPlatformEvent::eFWT_SaveStart;
			NotifyListeners(event);
		}
		if (m_bSaving && !m_delayLevelStartIcon)
		{
			IPlatformOS::SPlatformEvent event(0);
			event.m_eEventType = IPlatformOS::SPlatformEvent::eET_FileWrite;
			event.m_uParams.m_fileWrite.m_type = IPlatformOS::SPlatformEvent::eFWT_SaveEnd;
			NotifyListeners(event);
			m_bSaving = false;
		}
	}
}

IPlatformOS::ECDP_Start CPlatformOS_PC::StartUsingCachePaks(const int user, bool* outWritesOccurred)
{
	if (outWritesOccurred)
	{
		*outWritesOccurred = false;
	}

	if (m_cachePakStatus != 0)
	{
		CryLog("StartUsingCachePak '%d' ERROR already in use", user);
		return IPlatformOS::eCDPS_AlreadyInUse;
	}

	m_cachePakStatus = 1;
	m_cachePakUser = user;

	return IPlatformOS::eCDPS_Success;
}

IPlatformOS::ECDP_End CPlatformOS_PC::EndUsingCachePaks(const int user)
{
	const int currentCachePakStatus = m_cachePakStatus;
	const int currentCachePakUser = m_cachePakUser;
	m_cachePakStatus = 0;
	m_cachePakUser = -1;

	if (currentCachePakStatus != 1)
	{
		CryLog("EndUsingCachePaks '%d' ERROR not in use", user);
		return IPlatformOS::eCDPE_NotInUse;
	}

	if (user != currentCachePakUser)
	{
		CryLog("EndUsingCachePaks '%d' ERROR wrong user : current cache pak user:%d", user, currentCachePakUser);
		return IPlatformOS::eCDPE_WrongUser;
	}

	return IPlatformOS::eCDPE_Success;
}

IPlatformOS::ECDP_Open CPlatformOS_PC::DoesCachePakExist(const char* const filename, const size_t size, unsigned char md5[16])
{
	if (m_cachePakStatus != 1)
	{
		CryLog("OpenCachePak '%s' ERROR cache paks not ready to be used", filename);
		return IPlatformOS::eCDPO_NotInUse;
	}

	bool ret;
	unsigned char fileMD5[16];

	CryFixedStringT<128> realFileName;
	realFileName.Format("%%USER%%/cache/%s", filename);

	const uint32 fileOpenFlags = ICryPak::FLAGS_NEVER_IN_PAK | ICryPak::FLAGS_PATH_REAL | ICryPak::FOPEN_ONDISK;
	FILE* const file = gEnv->pCryPak->FOpen(realFileName.c_str(), "rbx", fileOpenFlags);
	if (file == NULL)
	{
		CryLog("DoesCachePakExist '%s' ERROR file not found '%s'", filename, realFileName.c_str());
		return IPlatformOS::eCDPO_FileNotFound;
	}
	gEnv->pCryPak->FSeek(file, 0, SEEK_END);
	const size_t fileSize = (size_t)(gEnv->pCryPak->FTell(file));
	gEnv->pCryPak->FClose(file);

	if (fileSize != size)
	{
		CryLog("DoesCachePakExist '%s' ERROR size doesn't match Cache:%" PRISIZE_T " Input:%" PRISIZE_T " '%s'", filename, fileSize, size, realFileName.c_str());
		return IPlatformOS::eCDPO_SizeNotMatch;
	}

	ret = gEnv->pCryPak->ComputeMD5(realFileName.c_str(), fileMD5, fileOpenFlags);
	if (ret == false)
	{
		CryLog("DoesCachePakExist '%s' ERROR during ComputeMD5 '%s'", filename, realFileName.c_str());
		return IPlatformOS::eCDPO_MD5Failed;
	}
	if (memcmp(fileMD5, md5, 16) != 0)
	{
		CryLog("DoesCachePakExist '%s' ERROR hashes don't match '%s'", filename, realFileName.c_str());
		CryLog("Computed MD5 %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		       fileMD5[0], fileMD5[1], fileMD5[2], fileMD5[3],
		       fileMD5[4], fileMD5[5], fileMD5[6], fileMD5[7],
		       fileMD5[8], fileMD5[9], fileMD5[10], fileMD5[11],
		       fileMD5[12], fileMD5[13], fileMD5[14], fileMD5[15]);
		CryLog("Manifest MD5 %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
		       md5[0], md5[1], md5[2], md5[3],
		       md5[4], md5[5], md5[6], md5[7],
		       md5[8], md5[9], md5[10], md5[11],
		       md5[12], md5[13], md5[14], md5[15]);

		return IPlatformOS::eCDPO_HashNotMatch;
	}

	return IPlatformOS::eCDPO_Success;
}

IPlatformOS::ECDP_Open CPlatformOS_PC::OpenCachePak(const char* const filename, const char* const bindRoot, const size_t size, unsigned char md5[16])
{
	bool ret;
	IPlatformOS::ECDP_Open existsResult = DoesCachePakExist(filename, size, md5);
	if (existsResult != IPlatformOS::eCDPO_Success)
	{
		return existsResult;
	}

	CryFixedStringT<128> realFileName;
	realFileName.Format("%%USER%%/cache/%s", filename);

	const uint32 fileOpenFlags = ICryPak::FLAGS_NEVER_IN_PAK | ICryPak::FLAGS_PATH_REAL | ICryPak::FOPEN_ONDISK;
	const uint32 pakOpenFlags = fileOpenFlags | ICryArchive::FLAGS_OVERRIDE_PAK;
	ret = gEnv->pCryPak->OpenPack(bindRoot, realFileName.c_str(), pakOpenFlags);
	if (ret == false)
	{
		CryLog("OpenCachePak '%s' ERROR during OpenPack '%s'", filename, realFileName.c_str());
		return IPlatformOS::eCDPO_OpenPakFailed;
	}

	return IPlatformOS::eCDPO_Success;
}

IPlatformOS::ECDP_Close CPlatformOS_PC::CloseCachePak(const char* const filename)
{
	if (m_cachePakStatus != 1)
	{
		CryLog("CloseCachePak '%s' ERROR cache paks not ready to be used", filename);
		return IPlatformOS::eCDPC_NotInUse;
	}

	CryFixedStringT<128> realFileName;
	realFileName.Format("%%USER%%/cache/%s", filename);
	const uint32 pakOpenFlags = ICryPak::FLAGS_NEVER_IN_PAK | ICryPak::FLAGS_PATH_REAL | ICryPak::FOPEN_ONDISK | ICryArchive::FLAGS_OVERRIDE_PAK;

	const bool ret = gEnv->pCryPak->ClosePack(realFileName.c_str(), pakOpenFlags);
	if (ret == false)
	{
		CryLog("CloseCachePak '%s' ERROR during ClosePack '%s'", filename, realFileName.c_str());
		return IPlatformOS::eCDPC_Failure;
	}
	return IPlatformOS::eCDPC_Success;
}

IPlatformOS::ECDP_Delete CPlatformOS_PC::DeleteCachePak(const char* const filename)
{
	if (m_cachePakStatus != 1)
	{
		CryLog("DeleteCachePak '%s' ERROR cache paks not ready to be used", filename);
		return IPlatformOS::eCDPD_NotInUse;
	}

	CryFixedStringT<128> realFileName;
	realFileName.Format("%%USER%%/cache/%s", filename);
	const bool ret = gEnv->pCryPak->RemoveFile(realFileName.c_str());
	if (ret == false)
	{
		CryLog("DeleteCachePak '%s' ERROR during RemoveFile '%s'", filename, realFileName.c_str());
		return IPlatformOS::eCDPD_Failure;
	}
	return IPlatformOS::eCDPD_Success;
}

IPlatformOS::ECDP_Write CPlatformOS_PC::WriteCachePak(const char* const filename, const void* const pData, const size_t numBytes)
{
	if (m_cachePakStatus != 1)
	{
		CryLog("WriteCachePak '%s' ERROR cache paks not ready to be used", filename);
		return IPlatformOS::eCDPW_NotInUse;
	}

	CryFixedStringT<128> realFileName;
	realFileName.Format("%%USER%%/cache/%s", filename);
	const uint32 nWriteFlags = ICryPak::FLAGS_PATH_REAL | ICryPak::FOPEN_ONDISK;
	FILE* file = gEnv->pCryPak->FOpen(realFileName.c_str(), "wb", nWriteFlags);
	if (file == NULL)
	{
		CryLog("WriteCachePak '%s' ERROR FOpen '%s'", filename, realFileName.c_str());
		return IPlatformOS::eCDPW_NoFreeSpace;
	}
	size_t numBytesWritten = gEnv->pCryPak->FWrite(pData, 1, numBytes, file);
	if (numBytesWritten != numBytes)
	{
		CryLog("WriteCachePak '%s' ERROR FWrite failed %" PRISIZE_T " written size %" PRISIZE_T " '%s'", filename, numBytesWritten, numBytes, realFileName.c_str());
		return IPlatformOS::eCDPW_Failure;
	}
	const int ret = gEnv->pCryPak->FClose(file);
	if (ret != 0)
	{
		CryLog("WriteCachePak '%s' ERROR FClose failed %d '%s'", filename, ret, realFileName.c_str());
		return IPlatformOS::eCDPW_Failure;
	}
	return IPlatformOS::eCDPW_Success;
}

bool CPlatformOS_PC::GetLocalIPAddress(char* ipAddress, uint32& ip, int length) const
{
	CRYSOCKADDR inet, addr;
	CRYSOCKET sock;

	memset(&inet, 0, sizeof(inet));
	inet.sa_family = AF_INET;
	inet.sa_data[0] = (unsigned char)0;
	inet.sa_data[1] = (unsigned char)79;
	inet.sa_data[2] = (unsigned char)0xC0;
	inet.sa_data[3] = (unsigned char)0xA8;
	inet.sa_data[4] = (unsigned char)0x01;
	inet.sa_data[5] = (unsigned char)0x01;

	// zero the address portion
	ip = 0;
	// create a temp socket (must be datagram)
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock != CRY_INVALID_SOCKET)
	{
		// connect to remote address
		if (CrySock::connect(sock, &inet, sizeof(inet)) == 0)
		{
			CRYSOCKLEN_T slen = sizeof(addr);
			// query the host address
			if (CrySock::getsockname(sock, &addr, &slen) == 0)
			{
				ip = (unsigned char)addr.sa_data[5];
				ip |= ((unsigned char)addr.sa_data[4]) << 8;
				ip |= ((unsigned char)addr.sa_data[3]) << 16;
				ip |= ((unsigned char)addr.sa_data[2]) << 24;
			}
			// close the socket
			CrySock::closesocket(sock);
		}
	}

	if (UIPToText(ip, ipAddress, length) == NULL)
		return false;

	return true;
}

IPlatformOS::IFileFinderPtr CPlatformOS_PC::GetFileFinder(unsigned int user)
{
	return IFileFinderPtr(new CFileFinderCryPak());
}

#endif
