// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameSerialize.h"

#include "XmlSaveGame.h"
#include "XmlLoadGame.h"

#include <CryGame/IGameFramework.h>
#include "IPlayerProfiles.h"

namespace
{
ISaveGame* CSaveGameCurrentUser()
{
	IPlayerProfileManager* pPPMgr = CCryAction::GetCryAction()->GetIPlayerProfileManager();
	if (pPPMgr)
	{
		int userCount = pPPMgr->GetUserCount();
		if (userCount > 0)
		{
			IPlayerProfileManager::SUserInfo info;
			if (pPPMgr->GetUserInfo(pPPMgr->GetCurrentUserIndex(), info))
			{
				IPlayerProfile* pProfile = pPPMgr->GetCurrentProfile(info.userId);
				if (pProfile)
					return pProfile->CreateSaveGame();
				else
					GameWarning("CSaveGameCurrentUser: User '%s' has no active profile. No save created.", info.userId);
			}
			else
				GameWarning("CSaveGameCurrentUser: Invalid logged in user. No save created.");
		}
		else
			GameWarning("CSaveGameCurrentUser: No User logged in. No save created.");
	}

	// can't save without a profile
	return NULL;
}

ILoadGame* CLoadGameCurrentUser()
{
	IPlayerProfileManager* pPPMgr = CCryAction::GetCryAction()->GetIPlayerProfileManager();
	if (pPPMgr)
	{
		int userCount = pPPMgr->GetUserCount();
		if (userCount > 0)
		{
			IPlayerProfileManager::SUserInfo info;
			if (pPPMgr->GetUserInfo(pPPMgr->GetCurrentUserIndex(), info))
			{
				IPlayerProfile* pProfile = pPPMgr->GetCurrentProfile(info.userId);
				if (pProfile)
					return pProfile->CreateLoadGame();
				else
					GameWarning("CLoadGameCurrentUser: User '%s' has no active profile. Can't load game.", info.userId);
			}
			else
				GameWarning("CSaveGameCurrentUser: Invalid logged in user. Can't load game.");
		}
		else
			GameWarning("CLoadGameCurrentUser: No User logged in. Can't load game.");
	}

	// can't load without a profile
	return NULL;
}

};

void CGameSerialize::RegisterFactories(IGameFramework* pFW)
{
	// save/load game factories
	REGISTER_FACTORY(pFW, "xml", CXmlSaveGame, false);
	REGISTER_FACTORY(pFW, "xml", CXmlLoadGame, false);
	//	REGISTER_FACTORY(pFW, "binary", CXmlSaveGame, false);
	//	REGISTER_FACTORY(pFW, "binary", CXmlLoadGame, false);

	pFW->RegisterFactory("xml", CLoadGameCurrentUser, false);
	pFW->RegisterFactory("xml", CSaveGameCurrentUser, false);

}
