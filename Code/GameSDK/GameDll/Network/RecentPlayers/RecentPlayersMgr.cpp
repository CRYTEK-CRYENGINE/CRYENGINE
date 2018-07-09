// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "RecentPlayersMgr.h"


#include "Utility/CryWatch.h"
#include "Network/Lobby/SessionNames.h"
#include "Network/Squad/SquadManager.h"
#include "UI/UIManager.h"
#include "UI/UICVars.h"
#include "Network/FriendManager/GameFriendsMgr.h"

/*static*/ TGameFriendId CRecentPlayersMgr::SRecentPlayerData::__s_friend_internal_ids = 0x10000;	// so our ids wont clash with gamefriends mgr ids

#if !defined(_RELEASE)
	#define RECENT_PLAYERS_MGR_DEBUG 1
#else
	#define RECENT_PLAYERS_MGR_DEBUG 0
#endif

CRecentPlayersMgr::CRecentPlayersMgr()
{
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	CRY_ASSERT(pGameLobby);
	pGameLobby->RegisterGameLobbyEventListener(this);

	CSquadManager *pSquadManager = g_pGame->GetSquadManager();
	CRY_ASSERT(pSquadManager);
	pSquadManager->RegisterSquadEventListener(this);

	m_localUserId = CryUserInvalidID;
}

CRecentPlayersMgr::~CRecentPlayersMgr()
{
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	if (pGameLobby)
	{
		pGameLobby->UnregisterGameLobbyEventListener(this);
	}

	CSquadManager *pSquadManager = g_pGame->GetSquadManager();
	if (pSquadManager)
	{
		pSquadManager->UnregisterSquadEventListener(this);
	}
}

void CRecentPlayersMgr::Update(const float inFrameTime)
{
	// add cvar driven CryWatches()
}

void CRecentPlayersMgr::RegisterRecentPlayersMgrEventListener(IRecentPlayersMgrEventListener *pListener)
{
	stl::push_back_unique(m_recentPlayersEventListeners, pListener);
}

void CRecentPlayersMgr::UnregisterRecentPlayersMgrEventListener(IRecentPlayersMgrEventListener *pListener)
{
	stl::find_and_erase(m_recentPlayersEventListeners, pListener);
}

void CRecentPlayersMgr::AddedSquaddie(CryUserID userId)
{
	AddOrUpdatePlayer(userId, NULL);
}

void CRecentPlayersMgr::UpdatedSquaddie(CryUserID userId)
{
	AddOrUpdatePlayer(userId, NULL);
}

void CRecentPlayersMgr::InsertedUser(CryUserID userId, const char *userName)
{
	AddOrUpdatePlayer(userId, userName);
}

const CRecentPlayersMgr::SRecentPlayerData *CRecentPlayersMgr::FindRecentPlayerDataFromInternalId(TGameFriendId inInternalId)
{
	const int numPlayers = m_players.size();
	const SRecentPlayerData *pFoundData=NULL;

	for (int i=0; i<numPlayers; i++)
	{
		const SRecentPlayerData *pPlayerData=&m_players[i];

		if (pPlayerData->m_internalId == inInternalId)
		{
			pFoundData=pPlayerData;
			break;
		}
	}

	return pFoundData;
}

void CRecentPlayersMgr::AddOrUpdatePlayer(CryUserID &inUserId, const char *inUserName)
{
	bool bUpdated=false;

	if (inUserId == CryUserInvalidID)
	{
		CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() ignoring invalid userId for userName=%s", inUserName ? inUserName : "NULL");
		return;
	}

	if (inUserId == m_localUserId)
	{
		CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() ignoring the local user id=%s; userName=%s", inUserId.get()->GetGUIDAsString().c_str(), inUserName ? inUserName : "NULL");
		return;
	}

	CGameFriendsMgr *pGameFriendsMgr = g_pGame->GetGameFriendsMgr();
	if (pGameFriendsMgr)
	{
		SGameFriendData *pGameFriendData = pGameFriendsMgr->GetFriendByCryUserId(inUserId);
		if (pGameFriendData)
		{
			if (pGameFriendData->IsFriend())
			{
				CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() ignoring someone who's already a friend - user id=%s; userName=%s", inUserId.get()->GetGUIDAsString().c_str(), inUserName ? inUserName : "NULL");
				return;
			}
		}
	}

	if (!inUserName)
	{
		CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() passed a null inUserName. Trying to find userName from userId=%s", inUserId.get()->GetGUIDAsString().c_str());

		if (CSquadManager *pSquadManager = g_pGame->GetSquadManager())
		{
			const SSessionNames *pSessionNames = pSquadManager->GetSessionNames();
			if(pSessionNames)
			{
				const SSessionNames::SSessionName *pSquaddieName = pSessionNames->GetSessionNameByUserId(inUserId);
				if (pSquaddieName)
				{
					inUserName = pSquaddieName->m_name;
				}
			}
		}
	}

	if (!inUserName)
	{
		CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() has failed to find a userName for inUserId=%s. Returning.", inUserId.get()->GetGUIDAsString().c_str());
		return;
	}
	
	CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() inUserId=%s; inUserName=%s", inUserId.get()->GetGUIDAsString().c_str(), inUserName);

	SRecentPlayerData *pPlayerData = FindRecentPlayerDataFromUserId(inUserId);
	if (pPlayerData)
	{
		CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() found already existing user");

		if (pPlayerData->m_name.compareNoCase(inUserName))
		{
			CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() found existing user's name is different. Updating from %s to %s", pPlayerData->m_name.c_str(), inUserName);
			pPlayerData->m_name = inUserName;
			bUpdated=true;
		}
	}
	else
	{
		CryLog("CRecentPlayersMgr::AddOrUpdatePlayer() need to add new user");
		if (m_players.isfull())
		{
			m_players.removeAt(0);
		}

		SRecentPlayerData newPlayerData(inUserId, inUserName);
		m_players.push_back(newPlayerData);
		bUpdated=true;
	}

	if (bUpdated)
	{
		EventRecentPlayersUpdated();
	}
}

CRecentPlayersMgr::SRecentPlayerData *CRecentPlayersMgr::FindRecentPlayerDataFromUserId(CryUserID &inUserId)
{
	const int numPlayers = m_players.size();
	SRecentPlayerData *pFoundData=NULL;

	for (int i=0; i<numPlayers; i++)
	{
		SRecentPlayerData *pPlayerData=&m_players[i];

		if (pPlayerData->m_userId == inUserId)
		{
			pFoundData=pPlayerData;
			break;
		}
	}

	return pFoundData;
}

void CRecentPlayersMgr::EventRecentPlayersUpdated()
{
	const size_t numListeners = m_recentPlayersEventListeners.size();
	for (size_t i = 0; i < numListeners; ++ i)
	{
		m_recentPlayersEventListeners[i]->UpdatedRecentPlayers();
	}
}

void CRecentPlayersMgr::OnUserChanged(CryUserID localUserId)
{
	m_players.clear();
	m_localUserId = localUserId;
}
