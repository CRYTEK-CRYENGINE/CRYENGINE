// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYERPROFILEIMPLFS_H__
#define __PLAYERPROFILEIMPLFS_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "PlayerProfileManager.h"

struct ICommonProfileImpl
{
	virtual ~ICommonProfileImpl(){}
	virtual void                   InternalMakeFSPath(CPlayerProfileManager::SUserEntry* pEntry, const char* profileName, string& outPath) = 0;
	virtual void                   InternalMakeFSSaveGamePath(CPlayerProfileManager::SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder) = 0;
	virtual CPlayerProfileManager* GetManager() = 0;
};

#if 0
template<class T>
class CCommonSaveGameHelper
{
public:
	CCommonSaveGameHelper(T* pImpl) : m_pImpl(pImpl) {}
	bool       GetSaveGames(CPlayerProfileManager::SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName);
	ISaveGame* CreateSaveGame(CPlayerProfileManager::SUserEntry* pEntry);
	ILoadGame* CreateLoadGame(CPlayerProfileManager::SUserEntry* pEntry);

protected:
	bool FetchMetaData(XmlNodeRef& root, CPlayerProfileManager::SSaveGameMetaData& metaData);
	T* m_pImpl;
};
#endif

class CPlayerProfileImplFS : public CPlayerProfileManager::IPlatformImpl, public ICommonProfileImpl
{
public:
	CPlayerProfileImplFS();

	// CPlayerProfileManager::IPlatformImpl
	virtual bool       Initialize(CPlayerProfileManager* pMgr);
	virtual void       Release() { delete this; }
	virtual bool       LoginUser(SUserEntry* pEntry);
	virtual bool       LogoutUser(SUserEntry* pEntry);
	virtual bool       SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name, bool initialSave = false, int /*reason*/ = ePR_All);
	virtual bool       LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name);
	virtual bool       DeleteProfile(SUserEntry* pEntry, const char* name);
	virtual bool       RenameProfile(SUserEntry* pEntry, const char* newName);
	virtual bool       GetSaveGames(SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName);
	virtual ISaveGame* CreateSaveGame(SUserEntry* pEntry);
	virtual ILoadGame* CreateLoadGame(SUserEntry* pEntry);
	virtual bool       DeleteSaveGame(SUserEntry* pEntry, const char* name);
	virtual bool       GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail);
	// ~CPlayerProfileManager::IPlatformImpl

	// ICommonProfileImpl
	void                           InternalMakeFSPath(SUserEntry* pEntry, const char* profileName, string& outPath);
	void                           InternalMakeFSSaveGamePath(SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder);
	virtual CPlayerProfileManager* GetManager() { return m_pMgr; }
	// ~ICommonProfileImpl

protected:
	virtual ~CPlayerProfileImplFS();

private:
	CPlayerProfileManager* m_pMgr;
};

class CPlayerProfileImplFSDir : public CPlayerProfileManager::IPlatformImpl, public ICommonProfileImpl
{
public:
	CPlayerProfileImplFSDir();

	// CPlayerProfileManager::IPlatformImpl
	virtual bool                Initialize(CPlayerProfileManager* pMgr);
	virtual void                Release() { delete this; }
	virtual bool                LoginUser(SUserEntry* pEntry);
	virtual bool                LogoutUser(SUserEntry* pEntry);
	virtual bool                SaveProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name, bool initialSave = false, int /*reason*/ = ePR_All);
	virtual bool                LoadProfile(SUserEntry* pEntry, CPlayerProfile* pProfile, const char* name);
	virtual bool                DeleteProfile(SUserEntry* pEntry, const char* name);
	virtual bool                RenameProfile(SUserEntry* pEntry, const char* newName);
	virtual bool                GetSaveGames(SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName);
	virtual ISaveGame*          CreateSaveGame(SUserEntry* pEntry);
	virtual ILoadGame*          CreateLoadGame(SUserEntry* pEntry);
	virtual bool                DeleteSaveGame(SUserEntry* pEntry, const char* name);
	virtual ILevelRotationFile* GetLevelRotationFile(SUserEntry* pEntry, const char* name);
	virtual bool                GetSaveGameThumbnail(SUserEntry* pEntry, const char* saveGameName, SThumbnail& thumbnail);
	virtual void                GetMemoryStatistics(ICrySizer* s);
	// ~CPlayerProfileManager::IPlatformImpl

	// ICommonProfileImpl
	virtual void                   InternalMakeFSPath(SUserEntry* pEntry, const char* profileName, string& outPath);
	virtual void                   InternalMakeFSSaveGamePath(SUserEntry* pEntry, const char* profileName, string& outPath, bool bNeedFolder);
	virtual CPlayerProfileManager* GetManager() { return m_pMgr; }
	// ~ICommonProfileImpl

protected:
	virtual ~CPlayerProfileImplFSDir();

private:
	CPlayerProfileManager* m_pMgr;
};

class CCommonSaveGameHelper
{
public:
	CCommonSaveGameHelper(ICommonProfileImpl* pImpl) : m_pImpl(pImpl) {}
	bool                GetSaveGames(CPlayerProfileManager::SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* altProfileName);
	ISaveGame*          CreateSaveGame(CPlayerProfileManager::SUserEntry* pEntry);
	ILoadGame*          CreateLoadGame(CPlayerProfileManager::SUserEntry* pEntry);
	bool                DeleteSaveGame(CPlayerProfileManager::SUserEntry* pEntry, const char* name);
	bool                GetSaveGameThumbnail(CPlayerProfileManager::SUserEntry* pEntry, const char* saveGameName, CPlayerProfileManager::SThumbnail& thumbnail);
	bool                MoveSaveGames(CPlayerProfileManager::SUserEntry* pEntry, const char* oldProfileName, const char* newProfileName);
	ILevelRotationFile* GetLevelRotationFile(CPlayerProfileManager::SUserEntry* pEntry, const char* name);

protected:
	bool FetchMetaData(XmlNodeRef& root, CPlayerProfileManager::SSaveGameMetaData& metaData);
	ICommonProfileImpl* m_pImpl;
};

namespace PlayerProfileImpl
{
extern const char* PROFILE_ROOT_TAG;
extern const char* PROFILE_NAME_TAG;
extern const char* PROFILE_LAST_PLAYED;

// a simple serializer. manages sections (XmlNodes)
// is used during load and save
// CSerializerXML no longer supports sections being within the root node. In future it could be modified to but for now because of BinaryXML loading on consoles the ProfileImplFSDir is preferred.
class CSerializerXML : public CPlayerProfileManager::IProfileXMLSerializer
{
public:
	CSerializerXML(XmlNodeRef& root, bool bLoading);

	// CPlayerProfileManager::ISerializer
	inline bool IsLoading() { return m_bLoading; }

	XmlNodeRef  CreateNewSection(CPlayerProfileManager::EPlayerProfileSection section, const char* name);
	void        SetSection(CPlayerProfileManager::EPlayerProfileSection section, XmlNodeRef& node);
	XmlNodeRef  GetSection(CPlayerProfileManager::EPlayerProfileSection section);

protected:
	bool       m_bLoading;
	XmlNodeRef m_root;
	XmlNodeRef m_sections[CPlayerProfileManager::ePPS_Num];
};

// some helpers
bool       SaveXMLFile(const string& filename, const XmlNodeRef& rootNode);
XmlNodeRef LoadXMLFile(const string& filename);
bool       IsValidFilename(const char* filename);

} // namespace PlayerProfileImpl

#endif
