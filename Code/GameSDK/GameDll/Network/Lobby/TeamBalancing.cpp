// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "TeamBalancing.h"
#include "PlaylistManager.h"
#include "GameLobby.h"

#define ENABLE_TEAM_BALANCING_DEBUG 0

#if ENABLE_TEAM_BALANCING_DEBUG
	#define FATAL_ASSERT(expr, ...) if (!(expr)) { CryFatalError("TeamBalancing - " __VA_ARGS__); }
	#define TEAM_BALANCE_LOG(...) CryLog("[TeamBalancing] " __VA_ARGS__);
#else
	#define FATAL_ASSERT(...) {}
	#define TEAM_BALANCE_LOG(...) {}
#endif

#define TEAM_BALANCING_GROUP_ID_INVALID		0xFF
#define TEAM_BALANCING_CLANS 0

//-----------------------------------------------------------------------------
CTeamBalancing::STeamBalancing_PlayerSlot::STeamBalancing_PlayerSlot()
{
	Reset();
}

//-----------------------------------------------------------------------------
void CTeamBalancing::STeamBalancing_PlayerSlot::Reset()
{
	m_uid = CryMatchMakingInvalidConnectionUID;
	m_skill = 0;
	m_previousScore = 0;
	m_teamId = 0;
	m_groupIndex = TEAM_BALANCING_GROUP_ID_INVALID;
	m_bLockedOnTeam = false;
	m_bUsed = false;
}

//-----------------------------------------------------------------------------
CTeamBalancing::STeamBalancing_Group::STeamBalancing_Group()
{
	Reset();
}

//-----------------------------------------------------------------------------
void CTeamBalancing::STeamBalancing_Group::Reset()
{
	m_clanTag.clear();
	m_leaderUID = 0;
	m_totalSkill = 0;
	m_totalPrevScore = 0;
	m_type = ePGT_Unknown;
	m_members.clear();
	m_teamId = 0;
	m_bLockedOnTeam = false;
	m_bPresentAtGameStart = false;
	m_bUsed = false;
}

//-----------------------------------------------------------------------------
void CTeamBalancing::STeamBalancing_Group::InitClan( CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex, const char *pClanName )
{
	m_bUsed = true;
	m_type = ePGT_Clan;
	m_clanTag = pClanName;
	AddMember(pTeamBalancing, playerIndex);
}

//-----------------------------------------------------------------------------
void CTeamBalancing::STeamBalancing_Group::InitIndividual( CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex )
{
	m_bUsed = true;
	m_type = ePGT_Individual;
	AddMember(pTeamBalancing, playerIndex);
}

//-----------------------------------------------------------------------------
void CTeamBalancing::STeamBalancing_Group::InitSquad( CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex, uint32 leaderUID )
{
	m_bUsed = true;
	m_type = ePGT_Squad;
	m_leaderUID = leaderUID;
	AddMember(pTeamBalancing, playerIndex);
}

//-----------------------------------------------------------------------------
void CTeamBalancing::STeamBalancing_Group::AddMember( CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex )
{
	m_members.push_back(playerIndex);

	STeamBalancing_PlayerSlot *pSlot = &pTeamBalancing->m_players[playerIndex];
	m_totalSkill += pSlot->m_skill;

	pSlot->m_groupIndex = pTeamBalancing->GetIndexFromGroup(this);
}

//-----------------------------------------------------------------------------
void CTeamBalancing::STeamBalancing_Group::RemoveMember( CTeamBalancing *pTeamBalancing, TPlayerIndex playerIndex )
{
	int numMembers = m_members.size();
	for (int i = 0; i < numMembers; ++ i)
	{
		if (m_members[i] == playerIndex)
		{
			m_members.removeAt(i);
			break;
		}
	}

	if (m_members.size() > 0)
	{
		STeamBalancing_PlayerSlot *pSlot = &pTeamBalancing->m_players[playerIndex];
		m_totalSkill -= pSlot->m_skill;

		pSlot->m_groupIndex = TEAM_BALANCING_GROUP_ID_INVALID;
	}
	else
	{
		Reset();
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::Init( SSessionNames *pSessionNames )
{
	m_pSessionNames = pSessionNames;
	Reset();
}

//-----------------------------------------------------------------------------
void CTeamBalancing::Reset()
{
	m_maxPlayers = 0;
	m_bGameIsBalanced = false;
	m_bGameHasStarted = false;
	m_bBalancedTeamsForced = false;

	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		m_players[i].Reset();
		m_groups[i].Reset();
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::AddPlayer( SSessionNames::SSessionName *pPlayer )
{
	STeamBalancing_PlayerSlot *pPlayerSlot = AddPlayer(pPlayer->m_conId);
	if(pPlayerSlot)
	{
		TEAM_BALANCE_LOG("Added player %s (uid=%d)", pPlayer->m_name, pPlayer->m_conId.m_uid);

		UpdatePlayer(pPlayerSlot, pPlayer, true);
	}
}

//-----------------------------------------------------------------------------
CTeamBalancing::STeamBalancing_PlayerSlot*  CTeamBalancing::AddPlayer( SCryMatchMakingConnectionUID uid )
{
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_PlayerSlot *pPlayerSlot = &m_players[i];
		if (!pPlayerSlot->m_bUsed)
		{
			pPlayerSlot->m_bUsed = true;
			pPlayerSlot->m_uid = uid;
			return pPlayerSlot;
		}
	}

	FATAL_ASSERT(false, "Run out of player slots");
	CryLog("[TeamBalancing] run out of player slots");

	return NULL;
}


//-----------------------------------------------------------------------------
void CTeamBalancing::RemovePlayer( const SCryMatchMakingConnectionUID &uid )
{
	STeamBalancing_PlayerSlot *pPlayerSlot = FindPlayerSlot(uid);

	FATAL_ASSERT(pPlayerSlot, "Couldn't find player to remove");
	if (pPlayerSlot)
	{
		TEAM_BALANCE_LOG("Removing player (uid=%d) from slot %d", uid.m_uid, GetIndexFromSlot(pPlayerSlot));
		if (pPlayerSlot->m_groupIndex != TEAM_BALANCING_GROUP_ID_INVALID)
		{
			STeamBalancing_Group *pGroup = &m_groups[pPlayerSlot->m_groupIndex];
			pGroup->RemoveMember(this, GetIndexFromSlot(pPlayerSlot));
		}
		pPlayerSlot->Reset();

		BalanceTeams();
	}
	else
	{
		CryLog("[TeamBalancing] Couldn't find player to remove (uid=%d)", uid.m_uid);
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::UpdatePlayer( STeamBalancing_PlayerSlot *pSlot, SSessionNames::SSessionName *pPlayer, bool bBalanceTeams )
{
	uint32 squadLeaderUID =  pPlayer->m_userData[eLUD_SquadId1] + (pPlayer->m_userData[eLUD_SquadId2] << 8);
	CryFixedStringT<CLAN_TAG_LENGTH> clanTag;

	if(squadLeaderUID == 0)
	{
		pPlayer->GetClanTagName(clanTag);
	}

	UpdatePlayer(pSlot, pPlayer->GetSkillRank(), squadLeaderUID, clanTag, true);
}

//-----------------------------------------------------------------------------
void CTeamBalancing::UpdatePlayer( STeamBalancing_PlayerSlot *pSlot, uint16 skillRank, uint32 squadLeaderUID, CryFixedStringT<CLAN_TAG_LENGTH> &clanTag, bool bBalanceTeams )
{
	pSlot->m_skill = skillRank;
	
	// Determine required slot type
	EPlayerGroupType requestedGroupType = ePGT_Individual;

	if (squadLeaderUID != 0)
	{
		requestedGroupType = ePGT_Squad;
	}
#if TEAM_BALANCING_CLANS
	else
	{
		// servers allocated via the arbitator probably get this wrong at the moment,
		CPlaylistManager *pPlaylistManager = g_pGame->GetPlaylistManager();
		int variantIdx = pPlaylistManager ?	pPlaylistManager->GetActiveVariantIndex() : -1;
		if (variantIdx >= 0)
		{
			if (pPlaylistManager->GetVariants()[variantIdx].m_allowClans)
			{
				if (!clanTag.empty())
				{
					requestedGroupType = ePGT_Clan;
				}
			}
		}
	}
#endif

	TPlayerIndex playerIndex = GetIndexFromSlot(pSlot);

	bool bNeedNewGroup = true;

	if (pSlot->m_groupIndex != TEAM_BALANCING_GROUP_ID_INVALID)
	{
		STeamBalancing_Group *pCurrentGroup = &m_groups[pSlot->m_groupIndex];
		if (pCurrentGroup->m_type == requestedGroupType)
		{
			switch (pCurrentGroup->m_type)
			{
			case ePGT_Individual:
				bNeedNewGroup = false;
				break;
			case ePGT_Clan:
				if (!strcmp(pCurrentGroup->m_clanTag.c_str(), clanTag.c_str()))
				{
					bNeedNewGroup = false;
				}
				break;
			case ePGT_Squad:
				if (pCurrentGroup->m_leaderUID == squadLeaderUID)
				{
					bNeedNewGroup = false;
				}
				break;
			}
		}

		if (bNeedNewGroup)
		{
			TEAM_BALANCE_LOG("Player %s leaving group type %d to join type %d", pPlayer->m_name, pCurrentGroup->m_type, requestedGroupType);
			pCurrentGroup->RemoveMember(this, playerIndex);
		}
	}
	else
	{
		TEAM_BALANCE_LOG("Player %s joining group type %d", pPlayer->m_name, requestedGroupType);
	}

	if (bNeedNewGroup)
	{
		switch (requestedGroupType)
		{
		case ePGT_Individual:
			{
				STeamBalancing_Group *pGroup = FindEmptyGroup();
				pGroup->InitIndividual(this, playerIndex);
			}
			break;
		case ePGT_Squad:
			{
				STeamBalancing_Group *pGroup = FindGroupBySquad(squadLeaderUID);
				if (pGroup)
				{
					pGroup->AddMember(this, playerIndex);
				}
				else
				{
					pGroup = FindEmptyGroup();
					pGroup->InitSquad(this, playerIndex, squadLeaderUID);
				}
			}
			break;
		case ePGT_Clan:
			{
				STeamBalancing_Group *pGroup = FindGroupByClan(clanTag.c_str());
				if (pGroup)
				{
					pGroup->AddMember(this, playerIndex);
				}
				else
				{
					pGroup = FindEmptyGroup();
					pGroup->InitClan(this, playerIndex, clanTag.c_str());
				}
			}
			break;
		}
	}

	if (bBalanceTeams)
	{
		BalanceTeams();
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::UpdatePlayer( SSessionNames::SSessionName *pPlayer, uint32 previousSkill )
{
	STeamBalancing_PlayerSlot *pSlot = FindPlayerSlot(pPlayer->m_conId);
	FATAL_ASSERT(pSlot, "Couldn't find player to update");
	if (pSlot)
	{
		uint32 newSkill = pPlayer->GetSkillRank();
		if (pSlot->m_groupIndex != TEAM_BALANCING_GROUP_ID_INVALID)
		{
			m_groups[pSlot->m_groupIndex].m_totalSkill += (newSkill - previousSkill);
		}

		UpdatePlayer(pSlot, pPlayer, true);
	}
	else
	{
		CryLog("[TeamBalancing] couldn't find player to update %s (uid=%d)", pPlayer->m_name, pPlayer->m_conId.m_uid);
	}
}

//-----------------------------------------------------------------------------
CTeamBalancing::STeamBalancing_PlayerSlot* CTeamBalancing::FindPlayerSlot( const SCryMatchMakingConnectionUID &conId )
{
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_PlayerSlot *pPlayerSlot = &m_players[i];
		if (pPlayerSlot->m_bUsed && (pPlayerSlot->m_uid.m_uid == conId.m_uid))
		{
			return pPlayerSlot;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
const CTeamBalancing::STeamBalancing_PlayerSlot* CTeamBalancing::FindPlayerSlot( const SCryMatchMakingConnectionUID &conId ) const
{
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		const STeamBalancing_PlayerSlot *pPlayerSlot = &m_players[i];
		if (pPlayerSlot->m_bUsed && (pPlayerSlot->m_uid.m_uid == conId.m_uid))
		{
			return pPlayerSlot;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
CTeamBalancing::TPlayerIndex CTeamBalancing::GetIndexFromSlot( STeamBalancing_PlayerSlot* pPlayerSlot ) const
{
	return (TPlayerIndex)(pPlayerSlot - &m_players[0]);
}

//-----------------------------------------------------------------------------
CTeamBalancing::TGroupIndex CTeamBalancing::GetIndexFromGroup( STeamBalancing_Group* pGroup ) const
{
	return (TGroupIndex )(pGroup - &m_groups[0]);
}

//-----------------------------------------------------------------------------
void CTeamBalancing::SetLobbyPlayerCounts( int maxPlayers )
{
	m_maxPlayers = maxPlayers;
}

//-----------------------------------------------------------------------------
void CTeamBalancing::OnPlayerSpawned( const SCryMatchMakingConnectionUID &uid )
{
	STeamBalancing_PlayerSlot *pSlot = FindPlayerSlot(uid);
	FATAL_ASSERT(pSlot);
	if (pSlot)
	{
		pSlot->m_bLockedOnTeam = true;
		if (pSlot->m_groupIndex != TEAM_BALANCING_GROUP_ID_INVALID)
		{
			STeamBalancing_Group *pGroup = &m_groups[pSlot->m_groupIndex];
			pGroup->m_bLockedOnTeam = true;
			int numMembers = pGroup->m_members.size();
			for (int i = 0; i < numMembers; ++ i)
			{
				STeamBalancing_PlayerSlot *pPlayer = &m_players[pGroup->m_members[i]];
				if (pPlayer->m_teamId == pGroup->m_teamId)
				{
					pPlayer->m_bLockedOnTeam = true;
				}
			}
		}
	}
	else
	{
		CryLog("[TeamBalancing] failed to find player slot (uid=%d)", uid.m_uid);
	}

	m_bGameHasStarted = true;
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		if (m_groups[i].m_bUsed)
		{
			m_groups[i].m_bPresentAtGameStart = true;
		}
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::OnPlayerSwitchedTeam( const SCryMatchMakingConnectionUID &uid, uint8 teamId )
{
	if(teamId)
	{
		STeamBalancing_PlayerSlot *pSlot = FindPlayerSlot(uid);
		if (pSlot)
		{
			pSlot->m_teamId = teamId;
		}
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::OnGameFinished(EUpdateTeamType updateType)
{
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		if(updateType == eUTT_Unlock)
		{
			m_players[i].m_bLockedOnTeam = false;
			m_groups[i].m_bLockedOnTeam = false;
		}
		else if(updateType == eUTT_Switch)
		{
			if(m_players[i].m_bLockedOnTeam)
			{
				m_players[i].m_teamId = 3 - m_players[i].m_teamId;
			}

			if(m_groups[i].m_bLockedOnTeam)
			{
				m_groups[i].m_teamId = 3 - m_groups[i].m_teamId;
			}
		}
		else
		{
			CRY_ASSERT_MESSAGE(0, string().Format("CTeamBalancing::OnGameFinished: unknown update type %d", updateType).c_str());
		}
		m_groups[i].m_bPresentAtGameStart = false;
	}

	m_bGameHasStarted = false;
	m_bBalancedTeamsForced = false;
	BalanceTeams();
}

//-----------------------------------------------------------------------------
uint8 CTeamBalancing::GetTeamId( const SCryMatchMakingConnectionUID &uid )
{
	STeamBalancing_PlayerSlot *pSlot = FindPlayerSlot(uid);
	if (pSlot)
	{
		return pSlot->m_teamId;
	}
	return 0;
}

//-----------------------------------------------------------------------------
bool CTeamBalancing::IsGameBalanced() const
{
	return m_bGameIsBalanced;
}

//-----------------------------------------------------------------------------
CTeamBalancing::STeamBalancing_Group * CTeamBalancing::FindGroupByClan( const char *pClanName )
{
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_Group *pGroup = &m_groups[i];
		if (pGroup->m_bUsed && (pGroup->m_type == ePGT_Clan) && !strcmp(pGroup->m_clanTag.c_str(), pClanName))
		{
			return pGroup;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
CTeamBalancing::STeamBalancing_Group * CTeamBalancing::FindGroupBySquad( uint32 leaderUID )
{
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_Group *pGroup = &m_groups[i];
		if (pGroup->m_bUsed && (pGroup->m_type == ePGT_Squad) && (pGroup->m_leaderUID == leaderUID))
		{
			return pGroup;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
CTeamBalancing::STeamBalancing_Group * CTeamBalancing::FindEmptyGroup()
{
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_Group *pGroup = &m_groups[i];
		if (!pGroup->m_bUsed)
		{
			return pGroup;
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
void CTeamBalancing::UpdatePlayerScores( SPlayerScores *pScores, int numScores )
{
	uint32 numPlayersToConsider = 0;
	uint32 totalScore = 0;
	uint32 totalSkill = 0;

	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_PlayerSlot *pSlot = &m_players[i];
		if (pSlot->m_bUsed)
		{
			pSlot->m_previousScore = 0;

			for (int j = 0; j < numScores; ++ j)
			{
				if (pScores[j].m_playerId.m_uid == pSlot->m_uid.m_uid)
				{
					if (pScores[j].m_fracTimeInGame > 0.75f)
					{
						pSlot->m_previousScore = pScores[j].m_score;
						totalScore += pScores[i].m_score;
						totalSkill += pSlot->m_skill;
						++ numPlayersToConsider;
					}
					break;
				}
			}
		}
	}
	
	if (numPlayersToConsider)
	{
		float averageScore = (float)(totalScore / numPlayersToConsider);
		float averageSkill = (float)(totalSkill / numPlayersToConsider);

		for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
		{
			STeamBalancing_PlayerSlot *pSlot = &m_players[i];
			if (pSlot->m_bUsed && (pSlot->m_previousScore == 0))
			{
				float frac = 0;
				if (totalSkill > 0)
				{
					frac = (float)pSlot->m_skill / averageSkill;
				}

				pSlot->m_previousScore = (uint32)(frac * averageScore);
			}
		}
	}
	else
	{
		for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
		{
			m_players[i].m_previousScore = m_players[i].m_skill;
		}
	}

	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_Group *pGroup = &m_groups[i];
		if (pGroup->m_bUsed)
		{
			pGroup->m_totalPrevScore = 0;
			int numMembers = pGroup->m_members.size();
			for (int j = 0; j < numMembers; ++ j)
			{
				pGroup->m_totalPrevScore += m_players[pGroup->m_members[j]].m_previousScore;
			}
		}
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::CreateBalanceGroups( SBalanceGroup *pGroups, int *pNumGroups, int *pNumPlayersOnTeams, int *pNumTotalPlayers, bool bAllowCommit )
{
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_Group *pGroup = &m_groups[i];
		if (pGroup->m_bUsed)
		{
			TEAM_BALANCE_LOG("CTeamBalancing::CreateBalanceGroups() pGroups=%p, i=%d, pNumGroups=%p(%d)", pGroups, i, pNumGroups, *pNumGroups);

			int numMembers = pGroup->m_members.size();
			(*pNumTotalPlayers) += numMembers;
			if (pGroup->m_bLockedOnTeam)
			{
				SBalanceGroup *pBalanceGroup = NULL;
				for (int j = 0; j < numMembers; ++ j)
				{
					STeamBalancing_PlayerSlot *pPlayer = &m_players[pGroup->m_members[j]];
					if (!pPlayer->m_bLockedOnTeam)
					{
						if (!pBalanceGroup)
						{
							pBalanceGroup = &pGroups[(*pNumGroups) ++];
							pBalanceGroup->m_bPresentAtGameStart = pGroup->m_bPresentAtGameStart;
							pBalanceGroup->m_desiredTeamId = pGroup->m_teamId;
						}
						pBalanceGroup->m_pPlayers[pBalanceGroup->m_numPlayers ++] = pPlayer;
						pBalanceGroup->m_totalScore += pPlayer->m_previousScore;
					}
					else
					{
						++ pNumPlayersOnTeams[pPlayer->m_teamId - 1];
					}
				}
				TEAM_BALANCE_LOG("  found locked group, contains %d unlocked players", pBalanceGroup ? pBalanceGroup->m_numPlayers : 0);
			}
			else
			{
				if (bAllowCommit)
				{
					pGroup->m_teamId = 0;
				}

				SBalanceGroup *pBalanceGroup = &pGroups[(*pNumGroups) ++];

				TEAM_BALANCE_LOG("  pBalanceGroup=%p, numMembers=%d", pBalanceGroup, numMembers);
				for (int j = 0; j < numMembers; ++ j)
				{
					STeamBalancing_PlayerSlot *pPlayer = &m_players[pGroup->m_members[j]];
					pBalanceGroup->m_pPlayers[j] = pPlayer;
					pBalanceGroup->m_totalScore += pPlayer->m_previousScore;
				}
				pBalanceGroup->m_numPlayers = numMembers;
				pBalanceGroup->m_bPresentAtGameStart = pGroup->m_bPresentAtGameStart;
				TEAM_BALANCE_LOG("  found unlocked group, contains %d players", pBalanceGroup->m_numPlayers);
			}
		}
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::AssignDesiredTeamGroups( SBalanceGroup *pGroups, int *pNumGroups, int *pNumPlayersOnTeams, int numTotalPlayers, bool bAllowCommit )
{
	int maxPlayersOnTeamInGame = (numTotalPlayers + 1) / 2;
	int maxPlayersOnTeamInLobby = (m_maxPlayers + 1) / 2;

	// First try to allocate any groups with a desired team id
	for (int i = 0; i < (*pNumGroups); ++ i)
	{
		SBalanceGroup *pGroup = &pGroups[i];
		int maxPlayersOnTeam = (m_bBalancedTeamsForced || (m_bGameHasStarted && !pGroup->m_bPresentAtGameStart)) ? maxPlayersOnTeamInGame : maxPlayersOnTeamInLobby;
		if (pGroup->m_desiredTeamId && ((pNumPlayersOnTeams[pGroup->m_desiredTeamId - 1] + pGroup->m_numPlayers) <= maxPlayersOnTeam))
		{
			TEAM_BALANCE_LOG("  group %d wants to be on team %d and fits", i, pGroup->m_desiredTeamId);
			if (bAllowCommit)
			{
				for (int j = 0; j < pGroup->m_numPlayers; ++ j)
				{
					pGroup->m_pPlayers[j]->m_teamId = pGroup->m_desiredTeamId;
				}
			}
			pNumPlayersOnTeams[pGroup->m_desiredTeamId - 1] += pGroup->m_numPlayers;

			pGroups[i] = pGroups[(*pNumGroups) - 1];
			-- (*pNumGroups);
			-- i;
		}
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::BalanceTeams()
{
	TEAM_BALANCE_LOG("CTeamBalancing::BalanceTeams");

	SBalanceGroup groupsToBalance[TEAM_BALANCING_MAX_PLAYERS];
	int numGroupsToBalance = 0;

	int teamMembers[2] = {0};
	int totalNumPlayers = 0;

	CreateBalanceGroups(groupsToBalance, &numGroupsToBalance, teamMembers, &totalNumPlayers, true);
	AssignDesiredTeamGroups(groupsToBalance, &numGroupsToBalance, teamMembers, totalNumPlayers, true);

	if (numGroupsToBalance)
	{
		std::sort(&groupsToBalance[0], &groupsToBalance[numGroupsToBalance]);

		int maxPlayersOnTeamInGame = (totalNumPlayers + 1) / 2;
		int maxPlayersOnTeamInLobby = (m_maxPlayers + 1) / 2;
		
		// Now allocate the rest of the groups
		for (int i = 0; i < numGroupsToBalance; ++ i)
		{
			SBalanceGroup *pGroup = &groupsToBalance[i];

			int maxPlayersOnTeam = (m_bBalancedTeamsForced || (m_bGameHasStarted && !pGroup->m_bPresentAtGameStart)) ? maxPlayersOnTeamInGame : maxPlayersOnTeamInLobby;

			int teamIdxToUse = (teamMembers[1] < teamMembers[0]) ? 1 : 0;
			const uint8 teamId = (teamIdxToUse + 1);
			TEAM_BALANCE_LOG("  trying to assign group %d (size %d) to team %d", i, pGroup->m_numPlayers, teamId);
			if ((teamMembers[teamIdxToUse] + pGroup->m_numPlayers) <= maxPlayersOnTeam)
			{
				TEAM_BALANCE_LOG("    fits (have %d members, %d already on team, %d allowed)", pGroup->m_numPlayers, teamMembers[teamIdxToUse], maxPlayersOnTeam);
				teamMembers[teamIdxToUse] += pGroup->m_numPlayers;
				for (int j = 0; j < pGroup->m_numPlayers; ++ j)
				{
					pGroup->m_pPlayers[j]->m_teamId = teamId;
				}
				m_groups[pGroup->m_pPlayers[0]->m_groupIndex].m_teamId = teamId;
			}
			else	// Need to split group :-(
			{
				const int membersForFirstTeam = (maxPlayersOnTeam - teamMembers[teamIdxToUse]);
				const int membersForSecondTeam = (pGroup->m_numPlayers - membersForFirstTeam);
				const uint8 secondTeamId = (3 - teamId);

				TEAM_BALANCE_LOG("    doesn't fit, %d to team %d, %d to team %d", membersForFirstTeam, teamId, membersForSecondTeam, secondTeamId);
				for (int playerIdx = 0; playerIdx < pGroup->m_numPlayers; ++ playerIdx)
				{
					if (playerIdx < membersForFirstTeam)
					{
						pGroup->m_pPlayers[playerIdx]->m_teamId = teamId;
					}
					else
					{
						pGroup->m_pPlayers[playerIdx]->m_teamId = secondTeamId;
					}
				}

				teamMembers[teamIdxToUse] += membersForFirstTeam;
				teamMembers[1 - teamIdxToUse] += membersForSecondTeam;
			}
		}
	}

	m_bGameIsBalanced = (abs(teamMembers[0] - teamMembers[1]) < 2);

#if ENABLE_TEAM_BALANCING_DEBUG
	TEAM_BALANCE_LOG("CTeamBalancing::BalanceTeams : dumping teams");
	for (int teamId = 1; teamId < 3; ++ teamId)
	{
		TEAM_BALANCE_LOG("  team %d", teamId);
		for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
		{
			STeamBalancing_PlayerSlot *pSlot = &m_players[i];
			if (pSlot->m_bUsed && pSlot->m_teamId == teamId)
			{
				SSessionNames::SSessionName *pSessionName = m_pSessionNames->GetSessionName(pSlot->m_uid, true);
				if (pSessionName)
				{
					TEAM_BALANCE_LOG("    %s (uid %u)", pSessionName->m_name, pSlot->m_uid.m_uid);
				}
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
void CTeamBalancing::ForceBalanceTeams()
{
	TEAM_BALANCE_LOG("ForceBalanceTeams");
	m_bBalancedTeamsForced = true;

#if USE_PC_PREMATCH
	for (int i = 0; i < TEAM_BALANCING_MAX_PLAYERS; ++ i)
	{
		STeamBalancing_Group *pGroup = &m_groups[i];
		if (pGroup->m_bUsed)
		{
			pGroup->m_bLockedOnTeam = false;
		}
	}
#endif

	BalanceTeams();
}

//-----------------------------------------------------------------------------
int CTeamBalancing::GetMaxNewSquadSize()
{
	TEAM_BALANCE_LOG("CTeamBalancing::GetMaxNewSquadSize()");

	SBalanceGroup groupsToBalance[TEAM_BALANCING_MAX_PLAYERS];
	int numGroupsToBalance = 0;

	int teamMembers[2] = {0};
	int totalNumPlayers = 0;

	CreateBalanceGroups(groupsToBalance, &numGroupsToBalance, teamMembers, &totalNumPlayers, false);
	AssignDesiredTeamGroups(groupsToBalance, &numGroupsToBalance, teamMembers, totalNumPlayers, false);

	// Attempt to fill the largest team
	int largestTeam = (teamMembers[1] > teamMembers[0]) ? 1 : 0;

	AssignMaxPlayersToTeam(groupsToBalance, &numGroupsToBalance, &teamMembers[largestTeam]);
	AssignMaxPlayersToTeam(groupsToBalance, &numGroupsToBalance, &teamMembers[1 - largestTeam]);
	
	int smallestTeam = (teamMembers[0] > teamMembers[1]) ? 1 : 0;
	return (m_maxPlayers / 2) - teamMembers[smallestTeam];
}

//-----------------------------------------------------------------------------
void CTeamBalancing::AssignMaxPlayersToTeam( SBalanceGroup *pGroups, int *pNumGroups, int *pNumPlayersOnTeam )
{
	int maxPlayersOnTeam = (m_maxPlayers / 2);
	CryLog("CTeamBalancing::AssignMaxPlayersToTeam(), starting with %d players, max=%d", (*pNumPlayersOnTeam), maxPlayersOnTeam);
	for (int i = 0; i < (*pNumGroups); ++ i)
	{
		SBalanceGroup *pGroup = &pGroups[i];
		if ((*pNumPlayersOnTeam) + pGroup->m_numPlayers <= maxPlayersOnTeam)
		{
			CryLog("  adding %d players to team", pGroup->m_numPlayers);
			*pNumPlayersOnTeam += pGroup->m_numPlayers;

			pGroups[i] = pGroups[*pNumGroups - 1];
			-- *pNumGroups;
			-- i;
		}
		else
		{
			CryLog("  group of %d doesn't fit", pGroup->m_numPlayers);
		}
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::WritePlayerToPacket(CCryLobbyPacket *pPacket, const SSessionNames::SSessionName *pPlayer)
{
	STeamBalancing_PlayerSlot *pPlayerSlot = FindPlayerSlot(pPlayer->m_conId);
	uint32 squadLeaderUID =  pPlayer->m_userData[eLUD_SquadId1] + (pPlayer->m_userData[eLUD_SquadId2] << 8);
	bool bLockedOnTeam = pPlayerSlot ? pPlayerSlot->m_bLockedOnTeam : false;

	pPacket->WriteConnectionUID(pPlayer->m_conId);
	pPacket->WriteUINT16(pPlayer->GetSkillRank());
	pPacket->WriteUINT32(squadLeaderUID);
	pPacket->WriteUINT8(pPlayerSlot ? pPlayerSlot->m_teamId : 0);
	pPacket->WriteBool(bLockedOnTeam);
}

//-----------------------------------------------------------------------------
void CTeamBalancing::ReadPlayerFromPacket(CCryLobbyPacket *pPacket, bool bBalanceTeams)
{
	SCryMatchMakingConnectionUID uid = pPacket->ReadConnectionUID();
	uint16 skillRank = pPacket->ReadUINT16();
	uint32 squadLeaderUID = pPacket->ReadUINT32();
	uint8 teamId = pPacket->ReadUINT8();
	bool bLockedOnTeam = pPacket->ReadBool();

	CryFixedStringT<CLAN_TAG_LENGTH> clanTag;

	// add player if necessary, maybe need to confirm add?
	STeamBalancing_PlayerSlot *pPlayerSlot = FindPlayerSlot(uid);
	if(!pPlayerSlot)
	{
		CryLog("ReadPlayerFromPacket adding uid %d", uid.m_uid);
		pPlayerSlot = AddPlayer(uid);
	}

	// update data
	if(pPlayerSlot)
	{
		pPlayerSlot->m_bLockedOnTeam = bLockedOnTeam;
		pPlayerSlot->m_teamId = teamId;

		UpdatePlayer(pPlayerSlot, skillRank, squadLeaderUID, clanTag, bBalanceTeams);

		CryLog("  updating player data uid %d teamId %d(%d) bLockedOnTeam %d(%d)", uid.m_uid, pPlayerSlot->m_teamId, teamId, pPlayerSlot->m_bLockedOnTeam, bLockedOnTeam);
	}
}

//------------------------------------------------------------------------------
void CTeamBalancing::WritePacket(CCryLobbyPacket *pPacket, GameUserPacketDefinitions packetType, SCryMatchMakingConnectionUID playerUID) 
{
	CGameLobby *pGameLobby = g_pGame->GetGameLobby();
	CRY_ASSERT(pGameLobby->IsServer());

	switch(packetType)
	{
		case eGUPD_TeamBalancingSetup:
		{
			const uint32 numPlayers = m_pSessionNames->Size();
			CryLog("  writing eGUPD_TeamBalancingSetup numPlayers %d", numPlayers);

			// PlayerData == uid + skill + squadid
			const uint32 dataSize = (CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT16Size + CryLobbyPacketUINT32Size + CryLobbyPacketUINT8Size + CryLobbyPacketBoolSize) * numPlayers;
			const uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketUINT32Size + dataSize;
			if(pPacket->CreateWriteBuffer(bufferSize))
			{
				pPacket->StartWrite(packetType, true);
				pPacket->WriteUINT32(numPlayers);

				for (uint32 i=0; i < numPlayers; ++i)
				{
					WritePlayerToPacket(pPacket, &m_pSessionNames->m_sessionNames[i]);
				}
			}
			break;
		}

		case eGUPD_TeamBalancingAddPlayer:
		{
			const SSessionNames::SSessionName *pPlayer = m_pSessionNames->GetSessionName(playerUID, false);

			CryLog("  writing eGUPD_TeamBalancingAddPlayer uid=%d", playerUID.m_uid);

			const uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT16Size + CryLobbyPacketUINT32Size + CryLobbyPacketUINT8Size + CryLobbyPacketBoolSize;
			if(pPacket->CreateWriteBuffer(bufferSize))
			{
				pPacket->StartWrite(packetType, true);
				WritePlayerToPacket(pPacket, pPlayer);
			}
			break;
		}
		
		case eGUPD_TeamBalancingRemovePlayer:
		{
			const uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketConnectionUIDSize;
			CryLog("  writing eGUPD_TeamBalancingRemovePlayer uid=%d", playerUID.m_uid);
			if(pPacket->CreateWriteBuffer(bufferSize))
			{
				pPacket->StartWrite(packetType, true);
				pPacket->WriteConnectionUID(playerUID);
			}

			break;
		}
		
		case eGUPD_TeamBalancingUpdatePlayer:
		{
			const SSessionNames::SSessionName *pPlayer = m_pSessionNames->GetSessionName(playerUID, false);

			CryLog("  writing eGUPD_TeamBalancingUpdatePlayer uid=%d", playerUID.m_uid);

			const uint32 bufferSize = CryLobbyPacketHeaderSize + CryLobbyPacketConnectionUIDSize + CryLobbyPacketUINT16Size + CryLobbyPacketUINT32Size + CryLobbyPacketUINT8Size + CryLobbyPacketBoolSize;
			if(pPacket->CreateWriteBuffer(bufferSize))
			{
				pPacket->StartWrite(packetType, true);
				WritePlayerToPacket(pPacket, pPlayer);
			}
			break;
		}

		default:
		{
			CRY_ASSERT_MESSAGE(0, string().Format("Unknown packet type %d passed to team balancing", packetType).c_str());
			break;
		}
	}
}

//-----------------------------------------------------------------------------
void CTeamBalancing::ReadPacket(CCryLobbyPacket *pPacket, uint32 packetType)
{
	switch(packetType)
	{
		case eGUPD_TeamBalancingSetup:
		{
			uint32 numPlayers = pPacket->ReadUINT32();
			CryLog("  reading eGUPD_TeamBalancingSetup numPlayers %d", numPlayers);
			for(uint32 i=0; i < numPlayers; ++i)
			{
				ReadPlayerFromPacket(pPacket, false);
			}
			//BalanceTeams();
			break;
		}

		case eGUPD_TeamBalancingAddPlayer:
		{
			CryLog("  reading eGUPD_TeamBalancingAddPlayer");
			ReadPlayerFromPacket(pPacket, false);
			break;
		}
		
		case eGUPD_TeamBalancingRemovePlayer:
		{
			SCryMatchMakingConnectionUID uid = pPacket->ReadConnectionUID();
			CryLog("  reading eGUPD_TeamBalancingRemovePlayer uid=%d", uid.m_uid);
			RemovePlayer(uid);
			break;
		}
		
		case eGUPD_TeamBalancingUpdatePlayer:
		{
			CryLog("  reading eGUPD_TeamBalancingUpdatePlayer");
			ReadPlayerFromPacket(pPacket, false);
			break;
		}

		default:
		{
			CRY_ASSERT_MESSAGE(0, string().Format("Unknown packet %d in team balancing", packetType).c_str());
			break;
		}
	}
}
