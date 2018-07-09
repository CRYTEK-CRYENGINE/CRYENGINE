// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "GameServerLists.h"
#include "Network/Lobby/GameBrowser.h"
#include "UI/ProfileOptions.h"
#include "GameLobbyData.h"

#if IMPLEMENT_PC_BLADES
/////////////////////////////////////////
CGameServerLists::CGameServerLists()
{
	Reset();

	IPlayerProfileManager *pProfileMgr = g_pGame->GetIGameFramework()->GetIPlayerProfileManager();
	if(pProfileMgr)
	{
		pProfileMgr->AddListener(this, true);
	}

	m_bHasChanges = false;

#if 0 && !defined(_RELEASE)
		REGISTER_COMMAND("gsl_addFavourite", CmdAddFavourite, 0, "Add a favourite server by name", false);
		REGISTER_COMMAND("gsl_removeFavourite", CmdRemoveFavourite, 0, "Remove a favourite server by name", false);
		REGISTER_COMMAND("gsl_listFavourite", CmdListFavourites, 0, "list favourite servers", false);
		REGISTER_COMMAND("gsl_showFavourite", CmdShowFavourites, 0, "show favourite servers", false);

		REGISTER_COMMAND("gsl_addRecent", CmdAddRecent, 0, "Add a recent server by name", false);
		REGISTER_COMMAND("gsl_removeRecent", CmdRemoveRecent, 0, "Remove a recent server by name", false);
		REGISTER_COMMAND("gsl_listRecent", CmdListRecent, 0, "list recent servers", false);
		REGISTER_COMMAND("gsl_showRecent", CmdShowRecent, 0, "show recent servers", false);
#endif
}

//---------------------------------------
CGameServerLists::~CGameServerLists()
{
#if !defined(_RELEASE)
	gEnv->pConsole->RemoveCommand("gsl_addFavourite");
	gEnv->pConsole->RemoveCommand("gsl_removeFavourite");
	gEnv->pConsole->RemoveCommand("gsl_listFavourite");
	gEnv->pConsole->RemoveCommand("gsl_showFavourite");

	gEnv->pConsole->RemoveCommand("gsl_addRecent");
	gEnv->pConsole->RemoveCommand("gsl_removeRecent");
	gEnv->pConsole->RemoveCommand("gsl_listRecent");
	gEnv->pConsole->RemoveCommand("gsl_showRecent");
#endif

	IPlayerProfileManager *pProfileMgr = g_pGame->GetIGameFramework()->GetIPlayerProfileManager();
	if(pProfileMgr)
	{
		pProfileMgr->RemoveListener(this);
	}
}

//---------------------------------------
void CGameServerLists::Reset()
{
	for(int i = 0; i < eGSL_Size; i++)
	{
		m_list[i].clear();
		m_rule[i].Reset();
	}
}

//---------------------------------------
const bool CGameServerLists::Add(const EGameServerLists list, const char* name, const uint32 favouriteId, bool bFromProfile)
{
	if(name && name[0])
	{
		//CryLog("[UI] CGameServerLists::Add %s %u", name, favouriteId);

		SServerInfoInt newServerInfo(name, favouriteId);
		m_rule[list].PreApply(&m_list[list], newServerInfo);
		m_list[list].push_front(newServerInfo);

		if (bFromProfile == false)
		{
			if (list == eGSL_Favourite)
			{
				m_bHasChanges = true;
			}
			else
			{
#if 0 // old frontend
				CFlashFrontEnd* pFlashFrontEnd = g_pGame->GetFlashMenu();
				if(pFlashFrontEnd)
				{
					if(!pFlashFrontEnd->IsGameActive())
					{
						CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions();
						if (pProfileOptions)
						{
							pProfileOptions->SaveProfile(ePR_All);
						}
					}
				}
#endif
			}
		}
		return true;
	}

	return false;
}

//---------------------------------------
const bool CGameServerLists::Remove(const EGameServerLists list, const uint32 favouriteId)
{
	//CryLog("[UI] CGameServerLists::Remove %u", favouriteId);

	SServerInfoInt removeInfo(NULL, favouriteId);
	m_list[list].remove(removeInfo);

	m_bHasChanges = true;

	return true;
}

//---------------------------------------
const int CGameServerLists::GetTotal(const EGameServerLists list) const
{
	return m_list[list].size();
}

//---------------------------------------
const bool CGameServerLists::InList(const EGameServerLists list, const uint32 favouriteId) const
{
	std::list<SServerInfoInt>::const_iterator it;
	std::list<SServerInfoInt>::const_iterator end = m_list[list].end();

	for (it = m_list[list].begin(); it != end; ++it)
	{
		if (it->m_favouriteId == favouriteId)
		{
			return true;
		}
	}
	return false;	
}

//---------------------------------------
void CGameServerLists::PopulateMenu(const EGameServerLists list) const
{
	if (CGameBrowser *pGameBrowser = g_pGame->GetGameBrowser())
	{
		uint32 numIds = 0;
		uint32 favouriteIdList[k_maxServersStoredInList];
		memset(favouriteIdList, INVALID_SESSION_FAVOURITE_ID, sizeof(favouriteIdList));

		std::list<SServerInfoInt>::const_iterator it;
		std::list<SServerInfoInt>::const_iterator end = m_list[list].end();

		for(it = m_list[list].begin(); it != end; ++it)
		{
			favouriteIdList[numIds] = it->m_favouriteId;
			++numIds;
		}

		pGameBrowser->StartFavouriteIdSearch(list, favouriteIdList, numIds );
	}
}

//---------------------------------------
void CGameServerLists::SaveChanges()
{
	if (m_bHasChanges)
	{
#if 0 // old frontend
		CFlashFrontEnd* pFlashFrontEnd = g_pGame->GetFlashMenu();
		if(pFlashFrontEnd)
		{
			if(!pFlashFrontEnd->IsGameActive())
			{
				CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions();
				if (pProfileOptions)
				{
					pProfileOptions->SaveProfile();
				}
			}
		}
#else
		CProfileOptions *pProfileOptions = g_pGame->GetProfileOptions();
		if (pProfileOptions)
		{
			pProfileOptions->SaveProfile();
		}
#endif
		m_bHasChanges = false;
	}
}

//---------------------------------------
void CGameServerLists::SaveToProfile(IPlayerProfile* pProfile, bool online, unsigned int reason)
{
	if(reason & ePR_Options)
	{
		for(int i = 0; i < eGSL_Size; i++)
		{
			//Save it out backwards so it adds them in the same order
			std::list<SServerInfoInt>::const_reverse_iterator it;
			std::list<SServerInfoInt>::const_reverse_iterator end = m_list[i].rend();
			int j = 0;
			int numServers = m_list[i].size();
			pProfile->SetAttribute(string().Format("MP/ServerLists/%d/Num", i).c_str(), numServers);
			for(it = m_list[i].rbegin(); it != end; ++it)
			{
				pProfile->SetAttribute(string().Format("MP/ServerLists/%d/%d/Name", i, j).c_str(), (*it).m_name);
				pProfile->SetAttribute(string().Format("MP/ServerLists/%d/%d/Id", i, j).c_str(), (*it).m_favouriteId);
				j++;
			}
		}
	}
}

//---------------------------------------
void CGameServerLists::LoadFromProfile(IPlayerProfile* pProfile, bool online, unsigned int reason)
{
	if(reason & ePR_Options)
	{
		Reset();

		for(int i = 0; i < eGSL_Size; i++)
		{
			int numServers = 0;
			if (pProfile->GetAttribute(string().Format("MP/ServerLists/%d/Num", i).c_str(), numServers))
			{
				for (int j = 0; j < numServers; ++ j)
				{
					string name("");
					if (pProfile->GetAttribute(string().Format("MP/ServerLists/%d/%d/Name", i, j).c_str(), name))
					{
						uint32 favouriteId = INVALID_SESSION_FAVOURITE_ID;
						if (pProfile->GetAttribute(string().Format("MP/ServerLists/%d/%d/Id", i, j).c_str(), favouriteId))
						{
							if ((!name.empty()) && (favouriteId != INVALID_SESSION_FAVOURITE_ID))
							{
								Add((EGameServerLists) i, name.c_str(), favouriteId, true);
							}
						}
					}
				}
			}
		}
	}
}

//---------------------------------------
void CGameServerLists::ServerFound( const SServerInfo &serverInfo, const EGameServerLists list, const uint32 favouriteId )
{
	//CryLog("[UI] CGameServerLists::ServerFound %s %d", serverInfo.m_hostName.c_str(), serverInfo.m_sessionFavouriteKeyId);

//TODO: "Forward this to UILobbyMP"
#if 0
	if (CMPMenuHub *pMPMenu = CMPMenuHub::GetMPMenuHub())
	{
		std::list<SServerInfo>::iterator it;
		std::list<SServerInfo>::const_iterator end = m_list[list].end();
		for (it = m_list[list].begin(); it != end; ++ it)
		{
			if (it->m_favouriteId == favouriteId)
			{
				pMPMenu->AddServer(serverInfo);
				break;
			}
		}
	}
#endif
}

//---------------------------------------
void CGameServerLists::ServerNotFound( const EGameServerLists list, const uint32 favouriteId )
{
	//CryLog("[UI] CGameServerLists::ServerNotFound %d", favouriteId);

//TODO: "Forward this to UILobbyMP"
#if 0
	if (CMPMenuHub *pMPMenu = CMPMenuHub::GetMPMenuHub())
	{
		std::list<SServerInfo>::iterator it;
		std::list<SServerInfo>::const_iterator end = m_list[list].end();
		for (it = m_list[list].begin(); it != end; ++ it)
		{
			if (it->m_favouriteId == favouriteId)
			{
				CUIServerList::SServerInfo serverInfo;
				serverInfo.m_hostName = it->m_name.c_str();
				serverInfo.m_sessionFavouriteKeyId = it->m_favouriteId;
				serverInfo.m_ping = 999;

				pMPMenu->AddServer(serverInfo);
				break;
			}
		}
	}
#endif
}

#if 0 && !defined(_RELEASE)
/////////////////////////////////////////
//static---------------------------------
void CGameServerLists::CmdAddFavourite(IConsoleCmdArgs* pCmdArgs)
{
	if(pCmdArgs->GetArgCount() == 2)
	{
		g_pGame->GetGameServerLists()->Add(CGameServerLists::eGSL_Favourite, pCmdArgs->GetArg(1));
	}
	else
	{
		CryLogAlways("Expected: gsl_addFavourite <server name>");
	}
}

//static---------------------------------
void CGameServerLists::CmdRemoveFavourite(IConsoleCmdArgs* pCmdArgs)
{
	if(pCmdArgs->GetArgCount() == 2)
	{
		g_pGame->GetGameServerLists()->Remove(CGameServerLists::eGSL_Favourite, pCmdArgs->GetArg(1));
	}
	else
	{
		CryLogAlways("Expected: gsl_removeFavourite <server name>");
	}
}

//static---------------------------------
void CGameServerLists::CmdListFavourites(IConsoleCmdArgs* pCmdArgs)
{
	CGameServerLists* pServer = g_pGame->GetGameServerLists();

	std::list<SServerInfo>::const_iterator it;
	std::list<SServerInfo>::const_iterator end = pServer->m_list[CGameServerLists::eGSL_Favourite].end();
	for(it = pServer->m_list[CGameServerLists::eGSL_Favourite].begin(); it != end; it++)
	{
		CryLogAlways("%s", (*it).m_name.c_str());
	}
}

//static---------------------------------
void CGameServerLists::CmdAddRecent(IConsoleCmdArgs* pCmdArgs)
{
	if(pCmdArgs->GetArgCount() == 2)
	{
		g_pGame->GetGameServerLists()->Add(CGameServerLists::eGSL_Recent, pCmdArgs->GetArg(1));
	}
	else
	{
		CryLogAlways("Expected: gsl_addRecent <server name>");
	}
}

//static---------------------------------
void CGameServerLists::CmdRemoveRecent(IConsoleCmdArgs* pCmdArgs)
{
	if(pCmdArgs->GetArgCount() == 2)
	{
		g_pGame->GetGameServerLists()->Remove(CGameServerLists::eGSL_Recent, pCmdArgs->GetArg(1));
	}
	else
	{
		CryLogAlways("Expected: gsl_removeRecent <server name>");
	}
}

//static---------------------------------
void CGameServerLists::CmdListRecent(IConsoleCmdArgs* pCmdArgs)
{
	CGameServerLists* pServer = g_pGame->GetGameServerLists();

	std::list<SServerInfo>::const_iterator it;
	std::list<SServerInfo>::const_iterator end = pServer->m_list[CGameServerLists::eGSL_Recent].end();
	for(it = pServer->m_list[CGameServerLists::eGSL_Recent].begin(); it != end; it++)
	{
		CryLogAlways("%s", (*it).m_name.c_str());
	}
}

//static---------------------------------
void CGameServerLists::CmdShowFavourites(IConsoleCmdArgs* pCmdArgs)
{
	CUIServerList* pUIServerList = g_pGame->GetFlashMenu()->GetMPMenu()->GetServerListUI();
	IFlashPlayer* pFlashPlayer = g_pGame->GetFlashMenu()->GetFlash();

	pUIServerList->ClearServerList(pFlashPlayer);

	g_pGame->GetGameServerLists()->PopulateMenu(CGameServerLists::eGSL_Favourite);
}

//static---------------------------------
void CGameServerLists::CmdShowRecent(IConsoleCmdArgs* pCmdArgs)
{
	CUIServerList* pUIServerList = g_pGame->GetFlashMenu()->GetMPMenu()->GetServerListUI();
	IFlashPlayer* pFlashPlayer = g_pGame->GetFlashMenu()->GetFlash();

	pUIServerList->ClearServerList(pFlashPlayer);

	g_pGame->GetGameServerLists()->PopulateMenu(CGameServerLists::eGSL_Recent);	
}
#endif //!defined(_RELEASE)

/////////////////////////////////////////
CGameServerLists::SServerInfoInt::SServerInfoInt(const char* name, uint32 favouriteId)
{
	m_name = name;
	m_favouriteId = favouriteId;
}

//---------------------------------------
bool CGameServerLists::SServerInfoInt::operator == (const SServerInfoInt & other) const
{
	return (m_favouriteId == other.m_favouriteId);
}

/////////////////////////////////////////
CGameServerLists::SListRules::SListRules()
{
	Reset();
}

//---------------------------------------
void CGameServerLists::SListRules::Reset()
{
	m_limit = k_maxServersStoredInList;
	m_unique = true;
}

//---------------------------------------
void CGameServerLists::SListRules::PreApply(std::list<SServerInfoInt>* pList, const SServerInfoInt &pNewInfo)
{
	if(m_limit)
	{
		//Will hit limit so pop now
		if(pList->size() >= m_limit)
		{
			pList->pop_back();
		}
	}

	if(m_unique)
	{
		//if it already exists remove it
		pList->remove(pNewInfo);
	}
}

#endif //IMPLEMENT_PC_BLADES