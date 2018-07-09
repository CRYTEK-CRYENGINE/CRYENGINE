// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   IPlatformOS_PC.h
//  Created:     18/12/2009 by Alex Weighell.
//  Description: Interface to the Platform OS
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PLATFORMOS_PC_H_
#define __PLATFORMOS_PC_H_

#if !CRY_PLATFORM_DURANGO

	#include <CryCore/Platform/IPlatformOS.h>
	#include <CryCore/Containers/CryListenerSet.h>
	#include <CryGame/IGameFramework.h>

namespace ZipDir
{
class FileEntryTree;

class CacheRW;
TYPEDEF_AUTOPTR(CacheRW);
typedef CacheRW_AutoPtr CacheRWPtr;
}

class CPlatformOS_PC : public IPlatformOS, public IPlatformOS::IPlatformListener, public ISystemEventListener, public IGameFrameworkListener
{
public:

	CPlatformOS_PC(const uint8 createParams);
	~CPlatformOS_PC();

	// ~IPlatformOS

	// Called each frame to update the platform listener
	virtual void        Tick(float realFrameTime);

	virtual const char* GetPlatformName() const { return "PC"; };

	// Local user profile functions to check/initiate user sign in:
	// See IPlatformOS.h for documentation.
	virtual unsigned int                                         UserGetMaximumSignedInUsers() const;
	virtual bool                                                 UserIsSignedIn(unsigned int userIndex) const;
	virtual bool                                                 UserIsSignedIn(const IPlatformOS::TUserName& userName, unsigned int& outUserIndex) const { if (!m_bSignedIn) return false; outUserIndex = 0; return true; }
	virtual bool                                                 UserDoSignIn(unsigned int numUsersRequested, unsigned int controllerIndex);
	virtual void                                                 UserSignOut(unsigned int user)                                                           {}
	virtual int                                                  GetFirstSignedInUser() const;
	virtual unsigned int                                         UserGetPlayerIndex(const char* userName) const                                           { return 0; }
	virtual bool                                                 UserGetName(unsigned int userIndex, IPlatformOS::TUserName& outName) const;
	virtual bool                                                 UserGetOnlineName(unsigned int userIndex, IPlatformOS::TUserName& outName) const;
	virtual bool                                                 UserSelectStorageDevice(unsigned int userIndex, bool bForceUI = false) { return true; } // always hard drive
	virtual bool                                                 GetUserProfilePreference(unsigned int user, IPlatformOS::EUserProfilePreference ePreference, SUserProfileVariant& outResult) const;
	virtual bool                                                 MountSaveFile(unsigned int userIndex)                                  { return true; }
	virtual IFileFinderPtr                                       GetFileFinder(unsigned int user);
	virtual void                                                 MountDLCContent(IDLCListener* pCallback, unsigned int user, const uint8 keyData[16]);

	virtual bool                                                 CanRestartTitle() const;
	virtual void                                                 RestartTitle(const char* pTitle);

	virtual bool                                                 UsePlatformSavingAPI() const;
	virtual bool                                                 BeginSaveLoad(unsigned int user, bool bSave);
	virtual void                                                 EndSaveLoad(unsigned int user);
	virtual IPlatformOS::ISaveReaderPtr                          SaveGetReader(const char* fileName, unsigned int user);
	virtual IPlatformOS::ISaveWriterPtr                          SaveGetWriter(const char* fileName, unsigned int user);

	virtual bool                                                 KeyboardStart(unsigned int inUserIndex, unsigned int flags, const char* title, const char* initialInput, int maxInputLength, IVirtualKeyboardEvents* pInCallback);
	virtual bool                                                 KeyboardIsRunning();
	virtual bool                                                 KeyboardCancel();

	virtual bool                                                 StringVerifyStart(const char* inString, IStringVerifyEvents* pInCallback);
	virtual bool                                                 IsVerifyingString();

	virtual void                                                 AddListener(IPlatformOS::IPlatformListener* pListener, const char* szName);
	virtual void                                                 RemoveListener(IPlatformOS::IPlatformListener* pListener);
	virtual void                                                 NotifyListeners(SPlatformEvent& event);

	virtual ILocalizationManager::EPlatformIndependentLanguageID GetSystemLanguage();
	virtual const char*                                          GetSKUId();
	virtual ILocalizationManager::TLocalizationBitfield          GetSystemSupportedLanguages();

	virtual bool                                                 PostLocalizationBootChecks();

	virtual void                                                 GetMemoryUsage(ICrySizer* pSizer) const;

	virtual bool                                                 DebugSave(SDebugDump& dump);

	virtual bool                                                 ConsoleLoadGame(IConsoleCmdArgs* pArgs);

	virtual const char* const                                    GetHostName();

	virtual void                                                 InitEncryptionKey(const char* pMagic, size_t magicLength, const uint8* pKey, size_t keyLength);

	virtual void                                                 GetEncryptionKey(const std::vector<char>** pMagic = NULL, const std::vector<uint8>** pKey = NULL);

	virtual EUserPIIStatus                                       GetUserPII(unsigned int inUser, SUserPII* pOutPII);

	virtual IPlatformOS::ECDP_Start                              StartUsingCachePaks(const int user, bool* outWritesOccurred);
	virtual IPlatformOS::ECDP_End                                EndUsingCachePaks(const int user);
	virtual IPlatformOS::ECDP_Open                               DoesCachePakExist(const char* const filename, const size_t size, unsigned char md5[16]);
	virtual IPlatformOS::ECDP_Open                               OpenCachePak(const char* const filename, const char* const bindRoot, const size_t size, unsigned char md5[16]);
	virtual IPlatformOS::ECDP_Close                              CloseCachePak(const char* const filename);
	virtual IPlatformOS::ECDP_Delete                             DeleteCachePak(const char* const filename);
	virtual IPlatformOS::ECDP_Write                              WriteCachePak(const char* const filename, const void* const pData, const size_t numBytes);

	virtual IPlatformOS::EZipExtractFail                         ExtractZips(const char* path);

	virtual void                                                 SetOpticalDriveIdle(bool bIdle);
	virtual void                                                 AllowOpticalDriveUsage(bool bAllow);

	virtual bool                                                 GetLocalIPAddress(char* ipAddress, uint32& ip, int length) const;

	// ~IPlatformOS

	// IPlatformOS::IPlatformListener
	virtual void OnPlatformEvent(const IPlatformOS::SPlatformEvent& _event);
	// ~IPlatformOS::IPlatformListener

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

	// IGameFrameworkListener
	virtual void OnPostUpdate(float fDeltaTime)    {}
	virtual void OnSaveGame(ISaveGame* pSaveGame)  {}
	virtual void OnLoadGame(ILoadGame* pLoadGame)  {}
	virtual void OnLevelEnd(const char* nextLevel) {}
	virtual void OnActionEvent(const SActionEvent& event);
	// ~IGameFrameworkListener

private:

	void                         SaveDirtyFiles();
	IPlatformOS::EZipExtractFail RecurseZipContents(ZipDir::FileEntryTree* pSourceDir, const char* currentPath, ZipDir::CacheRWPtr pCache);
	bool                         SxmlMissingFromHDD(ZipDir::FileEntryTree* pSourceDir, const char* currentPath, ZipDir::CacheRWPtr pCache);

	bool                         DecryptAndCheckSigning(const char* pInData, int inDataLen, char** pOutData, int* pOutDataLen, const uint8 key[16]);
	
private:
	CStableFPSWatcher                             m_fpsWatcher;
	CListenerSet<IPlatformOS::IPlatformListener*> m_listeners;
	std::vector<char>                             m_encryptionMagic;
	std::vector<uint8>                            m_encryptionKey;
	float m_delayLevelStartIcon;
	int   m_cachePakStatus;
	int   m_cachePakUser;
	bool  m_bSignedIn;
	bool  m_bSaving;
	bool  m_bLevelLoad;
	bool  m_bSaveDuringLevelLoad;
};

#endif
#endif
