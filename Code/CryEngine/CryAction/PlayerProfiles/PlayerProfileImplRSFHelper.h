// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYERPROFILEIMPLFSF_H__
#define __PLAYERPROFILEIMPLFSF_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "PlayerProfileImplFS.h"

class CRichSaveGameHelper
{
public:
	CRichSaveGameHelper(ICommonProfileImpl* pImpl) : m_pImpl(pImpl) {}
	bool       GetSaveGames(CPlayerProfileManager::SUserEntry* pEntry, CPlayerProfileManager::TSaveGameInfoVec& outVec, const char* profileName);
	ISaveGame* CreateSaveGame(CPlayerProfileManager::SUserEntry* pEntry);
	ILoadGame* CreateLoadGame(CPlayerProfileManager::SUserEntry* pEntry);
	bool       DeleteSaveGame(CPlayerProfileManager::SUserEntry* pEntry, const char* name);
	bool       GetSaveGameThumbnail(CPlayerProfileManager::SUserEntry* pEntry, const char* saveGameName, CPlayerProfileManager::SThumbnail& thumbnail);
	bool       MoveSaveGames(CPlayerProfileManager::SUserEntry* pEntry, const char* oldProfileName, const char* newProfileName);

protected:
	bool FetchMetaData(XmlNodeRef& root, CPlayerProfileManager::SSaveGameMetaData& metaData);
	ICommonProfileImpl* m_pImpl;
};

#endif
