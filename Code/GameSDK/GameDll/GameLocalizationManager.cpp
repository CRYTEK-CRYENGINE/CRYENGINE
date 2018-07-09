// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************

GameLocalizationManager is designed to look after the loading of strings
and be the sole place in the game that loads and unloads localization tags

When loading into the game we load <init> tag
Loading into singleplayer/multiplayer we load (and unload) <singleplayer>/<multiplayer> tags
Loading into a level we load <levelname> tags for sp and <gamemode name> for mp
Also as a special case credits are dynamically loaded
*************************************************************************/

#include "StdAfx.h"
#include "GameLocalizationManager.h"
#include "Network/Lobby/GameLobby.h"

CGameLocalizationManager::CGameLocalizationManager()
{
	LoadLocalizationData();

#if !defined(_RELEASE)
	REGISTER_COMMAND("LocalizationDumpLoadedTags", LocalizationDumpLoadedTags, VF_NULL, "Dump out into about the loaded localization tags");
#endif //#if !defined(_RELEASE)
}

CGameLocalizationManager::~CGameLocalizationManager()
{
	if(gEnv->pConsole)
	{
		gEnv->pConsole->RemoveCommand("LocalizationDumpLoadedTags");
	}
}

void CGameLocalizationManager::LoadLocalizationData()
{
	ILocalizationManager *pLocMan = GetISystem()->GetLocalizationManager();

	string locaFile = "Libs/Localization/localization.xml";

	CryLog("CGameLocalizationManager::LoadLocalizationData() is loading localization file=%s", locaFile.c_str());

	if (pLocMan->InitLocalizationData(locaFile.c_str()))
	{
		LoadTag(eLT_Init);
	}
	else
	{	
		LegacyLoadLocalizationData();
	}
}


void CGameLocalizationManager::LegacyLoadLocalizationData()
{
	// fallback to old system if localization.xml can not be found
	GameWarning("Using Legacy localization loading");

	ILocalizationManager *pLocMan = GetISystem()->GetLocalizationManager();

	string const szLocalizationFolderPath(PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR);
	string const search(szLocalizationFolderPath + "*.xml");

	ICryPak *pPak = gEnv->pCryPak;

	_finddata_t fd;
	intptr_t handle = pPak->FindFirst(search.c_str(), &fd);

	if (handle > -1)
	{
		do
		{
			CRY_ASSERT_MESSAGE(stricmp(PathUtil::GetExt(fd.name), "xml") == 0, "expected xml files only");

			string filename = szLocalizationFolderPath + fd.name;
			pLocMan->LoadExcelXmlSpreadsheet(filename.c_str());
		}
		while (pPak->FindNext(handle, &fd) >= 0);

		pPak->FindClose(handle);
	}
	else
	{
		GameWarning("Unable to find any Localization Data!");
	}
}

void CGameLocalizationManager::SetGameType()
{
	UnloadTag(eLT_GameType);
	LoadTag(eLT_GameType);
}

void CGameLocalizationManager::SetCredits( bool enable )
{
	if(enable)
	{
		LoadTag(eLT_Credits);
	}
	else
	{
		UnloadTag(eLT_Credits);
	}
}

void CGameLocalizationManager::LoadTag( ELocalizationTag tag )
{
	if(gEnv->IsEditor())// in editor all loca files are loaded during level load to get the info in the log
		return;

	switch(tag)
	{
	case eLT_Init:
		{
			LoadTagInternal(tag, "init");
		}
		break;
	case eLT_GameType:
		{
			LoadTagInternal(tag, gEnv->bMultiplayer ? "multiplayer" : "singleplayer");
		}
		break;
	case eLT_Credits:
		{
			LoadTagInternal(tag, "credits");
		}
		break;
	default:
		CRY_ASSERT_MESSAGE(0, "Unknown GameLocalizationManger Tag");
		break;
	}
}

void CGameLocalizationManager::LoadTagInternal( ELocalizationTag tag , const char* pTag )
{
	CRY_ASSERT(tag >= 0 && tag < eLT_Num);
	CRY_ASSERT(pTag);

	stack_string sTagLowercase(pTag);
	sTagLowercase.MakeLower();

	m_loadedTag[tag] = sTagLowercase.c_str();

	ILocalizationManager *pLocMan = GetISystem()->GetLocalizationManager();
	if(!pLocMan->LoadLocalizationDataByTag(sTagLowercase.c_str()))
	{
		GameWarning("Failed to load localization tag %s", sTagLowercase.c_str());
	}
}

void CGameLocalizationManager::UnloadTag( ELocalizationTag tag )
{
	if(gEnv->IsEditor())// in editor all loca files are loaded during level load to get the info in the log
		return;

	if(!m_loadedTag[tag].empty())
	{
		const char* pTag = m_loadedTag[tag].c_str();
		ILocalizationManager *pLocMan = GetISystem()->GetLocalizationManager();
		if(!pLocMan->ReleaseLocalizationDataByTag(pTag))
		{
			GameWarning("Failed to release localization tag %s", pTag);
		}

		m_loadedTag[tag].clear();
		CRY_ASSERT(m_loadedTag[tag].empty());
	}
}



//////////////////////////////////////////////////////////////////////////
#if !defined(_RELEASE)
void CGameLocalizationManager::LocalizationDumpLoadedTags( IConsoleCmdArgs* pArgs )
{
	CGameLocalizationManager* pMgr = g_pGame->GetGameLocalizationManager();
	for(uint32 i = 0; i < eLT_Num; i++)
	{
		if(!pMgr->m_loadedTag[i].empty())
		{
			CryLogAlways("[%d] %s", i, pMgr->m_loadedTag[i].c_str());
		}
	}
}

#endif //#if !defined(_RELEASE)