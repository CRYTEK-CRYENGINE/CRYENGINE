// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   PlayerProfileImplConsole.cpp
//  Created:     21/12/2009 by Alex Weighell.
//  Description: Player profile implementation for consoles which manage
//               the profile data via the OS and not via a file system
//               which may not be present.
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __PLAYERPROFILECONSOLE_H__
#define __PLAYERPROFILECONSOLE_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "PlayerProfileImplFS.h"

class CPlayerProfileImplConsole : public CPlayerProfileManager::IPlatformImpl, public ICommonProfileImpl
{
public:
	CPlayerProfileImplConsole();

	// CPlayerProfileManager::IPlatformImpl
	virtual bool                Initialize(CPlayerProfileManager* pMgr);
	virtual void                Release();
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
	virtual ~CPlayerProfileImplConsole();

private:
	CPlayerProfileManager* m_pMgr;
};

#endif
