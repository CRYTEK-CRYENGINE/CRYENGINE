// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 30:09:2009 : Created by James Bamford

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesSpawningBase.h"
#include "Game.h"
#include "GameRules.h"
#include "IGameRulesRoundsModule.h"
#include "IGameRulesPlayerStatsModule.h"
#include "IGameRulesObjectivesModule.h"

struct compare_spawns
{
	bool operator() (EntityId lhs, EntityId rhs ) const
	{
		int lhsT=g_pGame->GetGameRules()->GetTeam(lhs);
		int rhsT=g_pGame->GetGameRules()->GetTeam(rhs);
		if (lhsT == rhsT)
		{
			EntityId lhsG=g_pGame->GetGameRules()->GetSpawnLocationGroup(lhs);
			EntityId rhsG=g_pGame->GetGameRules()->GetSpawnLocationGroup(rhs);
			if (lhsG==rhsG)
			{
				IEntity *pLhs=gEnv->pEntitySystem->GetEntity(lhs);
				IEntity *pRhs=gEnv->pEntitySystem->GetEntity(rhs);

				return strcmp(pLhs->GetName(), pRhs->GetName())<0;
			}
			return lhsG<rhsG;
		}
		return lhsT<rhsT;
	}
};

struct compare_spawns_name_only
{
	bool operator() (EntityId lhs, EntityId rhs ) const
	{
		IEntity *pLhs=gEnv->pEntitySystem->GetEntity(lhs);
		IEntity *pRhs=gEnv->pEntitySystem->GetEntity(rhs);

		return strcmp(pLhs->GetName(), pRhs->GetName())<0;
	}
};

CGameRulesSpawningBase::CGameRulesSpawningBase()
{
	m_pGameRules=NULL;
	m_activeInitialSpawnGroupIndex = kInvalidInitialSpawnIndex;
	m_bTeamAlwaysUsesInitialSpawns[0] = false;
	m_bTeamAlwaysUsesInitialSpawns[1] = false;
	m_spawnGroupOverride = -1; 
}

CGameRulesSpawningBase::~CGameRulesSpawningBase()
{

}

void CGameRulesSpawningBase::Init(XmlNodeRef xml)
{
	m_pGameRules = g_pGame->GetGameRules();

	int ival;
	if (xml->getAttr("team1AlwaysUsesInitialSpawns", ival))
	{
		m_bTeamAlwaysUsesInitialSpawns[0] = (ival != 0);
	}
	if (xml->getAttr("team2AlwaysUsesInitialSpawns", ival))
	{
		m_bTeamAlwaysUsesInitialSpawns[1] = (ival != 0);
	}
}

void CGameRulesSpawningBase::PostInit()
{

}

void CGameRulesSpawningBase::Update(float frameTime)
{

}

//AddSpawnLocation and RemoveSpawnLocation should not be called at run time as they will corrupt the networking index data
void CGameRulesSpawningBase::AddSpawnLocation(EntityId location, bool isInitialSpawn, bool doVisTest, const char *pGroupName)
{
	if (isInitialSpawn)
	{
		TSpawnLocations *pGroup = GetSpawnLocationsFromGroup(m_initialSpawnLocations, pGroupName);
		stl::push_back_unique(*pGroup, location);
		if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
			CryLog("[SPAWN POINT]Adding initial spawn point to game rules, Id = '%d' (group='%s'). Number of initial Spawn points in group = '%" PRISIZE_T "'", location, pGroupName, pGroup->size());
	}
	else
	{
		stl::push_back_unique(m_spawnLocations, location);
		if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
			CryLog("[SPAWN POINT]Adding spawn point to game rules, Id = '%d'. Number of Spawn points = '%" PRISIZE_T "'", location, m_spawnLocations.size());
	}

	stl::push_back_unique(m_allSpawnLocations, location);
	std::sort(m_allSpawnLocations.begin(), m_allSpawnLocations.end(), compare_spawns_name_only());	// We now rely on the order of the table to save memory by not binding spawn points to the network
}

void CGameRulesSpawningBase::RemoveSpawnLocation(EntityId id, bool isInitialSpawn)
{
	if (isInitialSpawn)
	{
		int numGroups = m_initialSpawnLocations.size();
		for (int i = 0; i < numGroups; ++ i)
		{
			if (stl::find_and_erase(m_initialSpawnLocations[i].m_locations, id))
			{
				if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
				{
					CryLog("[SPAWN POINT]Removing initial spawn point from game rules, Id = '%d'. Number of initial Spawn points in group = '%" PRISIZE_T "'", id, m_initialSpawnLocations[i].m_locations.size()); 
				}
				break;
			}
		}
	}
	else
	{
		stl::find_and_erase(m_spawnLocations, id);

		if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
			CryLog("[SPAWN POINT]Removing spawn point from game rules, Id = '%d'. Number of Spawn points = '%" PRISIZE_T "'", id, m_spawnLocations.size()); 
	}

	stl::find_and_erase(m_allSpawnLocations, id);
	std::sort(m_allSpawnLocations.begin(), m_allSpawnLocations.end(), compare_spawns_name_only());	// We now rely on the order of the table to save memory by not binding spawn points to the network
}

//EnableSpawnLocation and DisableSpawnLocation are used at real time. They update the 'active' list of spawns, but they do not alter
//	the full list of spawns which is used for networking, to avoid corrupting the index data
void CGameRulesSpawningBase::EnableSpawnLocation(EntityId location, bool isInitialSpawn, const char *pGroupName)
{
	if (isInitialSpawn)
	{
		TSpawnLocations *pGroup = GetSpawnLocationsFromGroup(m_initialSpawnLocations, pGroupName);
		stl::push_back_unique(*pGroup, location);
		if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
			CryLog("[SPAWN POINT] Enabling initial spawn point in game rules, Id = '%d'. Number of initial Spawn points in group = '%" PRISIZE_T "'", location, pGroup->size());
	}
	else
	{
		stl::push_back_unique(m_spawnLocations, location);
		if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
			CryLog("[SPAWN POINT] Enabling spawn point to game rules, Id = '%d'. Number of Spawn points = '%" PRISIZE_T "'", location, m_spawnLocations.size());
	}
}

void CGameRulesSpawningBase::DisableSpawnLocation(EntityId id, bool isInitialSpawn)
{
	if (isInitialSpawn)
	{
		int numGroups = m_initialSpawnLocations.size();
		for (int i = 0; i < numGroups; ++ i)
		{
			if (stl::find_and_erase(m_initialSpawnLocations[i].m_locations, id))
			{
				if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
				{
					CryLog("[SPAWN POINT]Disabling initial spawn point in game rules, Id = '%d'. Number of initial Spawn points in group = '%" PRISIZE_T "'", id, m_initialSpawnLocations[i].m_locations.size()); 
				}
				break;
			}
		}
	}
	else
	{
		stl::find_and_erase(m_spawnLocations, id);
		if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
			CryLog("[SPAWN POINT]Removing spawn point from game rules, Id = '%d'. Number of Spawn points = '%" PRISIZE_T "'", id, m_spawnLocations.size()); 
	}
}

EntityId CGameRulesSpawningBase::GetSpawnLocation(EntityId playerId)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::GetSpawnLocation() is NOT implemented and shouldn't be called until it has been"));
	return 0;
}

EntityId CGameRulesSpawningBase::GetFirstSpawnLocation(int teamId) const
{
	if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
	{
		CryLog("[SPAWN POINT]Retrieving first spawn point at start for teamId '%d'. Number of spawn points = '%" PRISIZE_T "'", teamId, m_spawnLocations.size());
	}
	if (!m_spawnLocations.empty())
	{
		TSpawnLocations::const_iterator endIt=m_spawnLocations.end();
		for (TSpawnLocations::const_iterator it=m_spawnLocations.begin(); it!=endIt; ++it)
		{
			if (teamId==m_pGameRules->GetTeam(*it)) // && (!groupId || groupId==GetSpawnLocationGroup(*it)))	// no groups yet
				return *it;
		}
	}

	return 0;
}

void CGameRulesSpawningBase::SetLastSpawn(EntityId playerId, EntityId spawnId)
{
	TPlayerDataMap::iterator it = m_playerValues.find(playerId);

	if (it != m_playerValues.end())
	{			
		it->second.lastSpawnLocationId = spawnId;
	}
}

int CGameRulesSpawningBase::GetSpawnLocationCount() const
{
	return m_allSpawnLocations.size();
}

EntityId CGameRulesSpawningBase::GetNthSpawnLocation(int idx) const
{
	if (idx>=0 && idx<(int)m_allSpawnLocations.size())
		return m_allSpawnLocations[idx];
	return 0;
}

int CGameRulesSpawningBase::GetSpawnIndexForEntityId(EntityId spawnId) const
{
	if (!m_allSpawnLocations.empty())
	{
		int rVal=0;
		TSpawnLocations::const_iterator itEnd = m_allSpawnLocations.end();
		for (TSpawnLocations::const_iterator it=m_allSpawnLocations.begin(); it!=itEnd; ++it,++rVal)
		{
			if (*it == spawnId)
				return rVal;
		}
	}
	return -1;
}

void CGameRulesSpawningBase::AddAvoidPOI(EntityId entityId, float avoidDistance, bool enabled, bool bStaticPOI)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::AddAvoidPOI() is NOT implemented and shouldn't be called until it has been"));
}

void CGameRulesSpawningBase::RemovePOI(EntityId entityId)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::RemovePOI() is NOT implemented and shouldn't be called until it has been"));
}
	
void CGameRulesSpawningBase::EnablePOI(EntityId entityId)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::EnablePOI() is NOT implemented and shouldn't be called until it has been"));

}

void CGameRulesSpawningBase::DisablePOI(EntityId entityId)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::DisablePOI() is NOT implemented and shouldn't be called until it has been"));
}

void CGameRulesSpawningBase::PlayerJoined(EntityId playerId)
{
	m_playerValues.insert(TPlayerDataMap::value_type(playerId, SPlayerData()));
}

void CGameRulesSpawningBase::PlayerLeft(EntityId playerId)
{
	TPlayerDataMap::iterator it = m_playerValues.find(playerId);
	if (it != m_playerValues.end())
	{
		m_playerValues.erase(it);
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesSpawningBase::PlayerLeft() failed to find a playervalue for player. This should not happen");
	}
}

void CGameRulesSpawningBase::OnPlayerKilled(const HitInfo &hitInfo)
{
	IEntity *targetEntity = gEnv->pEntitySystem->GetEntity(hitInfo.targetId);

	if (targetEntity)
	{
		TPlayerDataMap::iterator it = m_playerValues.find(hitInfo.targetId);
		if (it != m_playerValues.end())
		{
			it->second.deathTime = GetTime();
			it->second.deathPos = targetEntity->GetWorldPos();
			it->second.lastKillerId = hitInfo.shooterId;
		}
		else
		{
			CRY_ASSERT_MESSAGE(0, string().Format("CGameRulesSpawningBase::OnPlayerKilled() failed to find a playervalue for player %s. This should not happen", targetEntity->GetName()));
		}
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesSpawningBase::OnPlayerKilled() failed to find a targetEntity for player that died");
	}
}

void CGameRulesSpawningBase::ClRequestRevive(EntityId playerId)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::ClRequestRevive() is NOT implemented and shouldn't be called until it has been"));
}

bool CGameRulesSpawningBase::SvRequestRevive(EntityId playerId, EntityId preferredSpawnId)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::SvRequestRevive() is NOT implemented and shouldn't be called until it has been"));
	return false;
}

void CGameRulesSpawningBase::PerformRevive(EntityId playerId, int teamId, EntityId preferredSpawnId)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::PerformRevive() is NOT implemented and shouldn't be called until it has been"));
}

void CGameRulesSpawningBase::ReviveAllPlayers(bool isReset, bool bOnlyIfDead)
{
	CRY_ASSERT_TRACE(0, ("CGameRulesSpawningBase::ReviveAllPlayers() is NOT implemented and shouldn't be called until it has been. Shouldn't be needed in singleplayer"));
}

CGameRulesSpawningBase::TSpawnLocations& CGameRulesSpawningBase::GetSpawnLocations(const EntityId playerId)
{
	if (m_initialSpawnLocations.empty())
	{
		return m_spawnLocations;
	}

	if (IsPlayerInitialSpawning(playerId))
	{
		return m_initialSpawnLocations[m_activeInitialSpawnGroupIndex].m_locations;
	}
	return m_spawnLocations;
}

// warning - this is only whether we expect to perform initial spawning
// this shouldn't be used to state that a player's spawn point IS initially spawning
// instead ensure an index can be found from the initial spawning list with GetSpawnIndexForEntityId(spawnId, true)
bool CGameRulesSpawningBase::IsPlayerInitialSpawning(const EntityId playerId) const
{
	if (m_initialSpawnLocations.empty())
	{
		return false;
	}

	int teamId = m_pGameRules->GetTeam(playerId);
	if ((teamId == 1) || (teamId == 2))
	{
		if (m_bTeamAlwaysUsesInitialSpawns[teamId - 1])
		{
			return true;
		}
	}

	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);
	if (pActor)
	{
		IGameRulesPlayerStatsModule *pPlayerStatsModule = m_pGameRules->GetPlayerStatsModule();
		const SGameRulesPlayerStat *pPlayerStats = pPlayerStatsModule ? pPlayerStatsModule->GetPlayerStats(playerId) : NULL;
		if (pPlayerStats)
		{
			IGameRulesRoundsModule *pRoundsModule = m_pGameRules->GetRoundsModule();
			if (!pRoundsModule || ((pRoundsModule->GetRoundNumber() == 0) && !pRoundsModule->IsRestarting()))
			{
				// Not rounds based or this is the first round, return true for players with the PLYSTATFL_USEINITIALSPAWNS
				// flag who haven't spawned yet
				if ((pPlayerStats->flags & SGameRulesPlayerStat::PLYSTATFL_USEINITIALSPAWNS) && 
						((pPlayerStats->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISGAME) == 0))
				{
					return true;
				}
			}
			else if (pRoundsModule->IsRestarting())
			{
				return true;
			}
			else
			{
				// Rounds based game and this isn't the first round, return true for players who were there at the
				// start of the round but haven't spawned yet
				if ((pPlayerStats->flags & SGameRulesPlayerStat::PLYSTATFL_HASHADROUNDRESTART) &&
						((pPlayerStats->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND) == 0))
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool CGameRulesSpawningBase::CanPlayerSpawnThisRound(const EntityId playerId) const
{
	bool allowed=true;

	IActor *pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);

	if (pActor->IsPlayer())
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		IGameRulesPlayerStatsModule *playerStats = pGameRules ? pGameRules->GetPlayerStatsModule() : NULL;

		if (playerStats)
		{
			const SGameRulesPlayerStat *stats = playerStats->GetPlayerStats(playerId);
			if (stats)
			{
				if (stats->flags & SGameRulesPlayerStat::PLYSTATFL_CANTSPAWNTHISROUND)
				{	
					allowed=false;
				}
			}
		}
	}

	CryLog("CGameRulesSpawningBase::CanPlayerSpawnThisRound() player=%s allowed=%d", pActor->GetEntity()->GetName(), allowed);
	return allowed;
}

CGameRulesSpawningBase::TSpawnLocations* CGameRulesSpawningBase::GetSpawnLocationsFromGroup( TSpawnGroups &groups, const char *pGroupName )
{
	uint32 spawnGroupId = CCrc32::ComputeLowercase(pGroupName);
	int numGroups = groups.size();
	TSpawnLocations *pGroup = NULL;
	for (int i = 0; i < numGroups; ++ i)
	{
		if (groups[i].m_id == spawnGroupId)
		{
			pGroup = &groups[i].m_locations;
			break;
		}
	}
	if (!pGroup)
	{
		groups.push_back(SSpawnGroup());
		groups[numGroups].m_id = spawnGroupId;
		groups[numGroups].m_useCount = 0;
		pGroup = &groups[numGroups].m_locations;

		if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
		{
			CryLog("[SPAWN POINT] Adding new spawn group, name='%s', id='%u'", pGroupName, spawnGroupId);
		}
	}
	return pGroup;
}

bool CGameRulesSpawningBase::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == eEA_GameServerD)
	{
		uint32 groupId = 0;
		if (m_initialSpawnLocations.size() > m_activeInitialSpawnGroupIndex)
		{
			groupId = m_initialSpawnLocations[m_activeInitialSpawnGroupIndex].m_id;
		}
		ser.Value("initialSpawnId", groupId, 'ui32');

		if (ser.IsReading())
		{
			int numGroups = m_initialSpawnLocations.size();
			for (int i = 0; i < numGroups; ++ i)
			{
				if (m_initialSpawnLocations[i].m_id == groupId)
				{
					if (i != m_activeInitialSpawnGroupIndex)
					{
						m_activeInitialSpawnGroupIndex = i;
						IGameRulesObjectivesModule *pObjectives = m_pGameRules->GetObjectivesModule();
						if (pObjectives)
						{
							pObjectives->OnNewGroupIdSelected(groupId);
						}
					}
					break;
				}
			}
		}
	}
	return true;
}

void CGameRulesSpawningBase::SelectNewInitialSpawnGroup()
{
	if (gEnv->bServer)
	{
		const int numGroups = m_initialSpawnLocations.size();
		if(m_spawnGroupOverride > -1 && m_spawnGroupOverride < numGroups)
		{
			SSpawnGroup &group = m_initialSpawnLocations[m_spawnGroupOverride];
			if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
			{
				CryLog("[SPAWN POINT] Activating spawn group via override %u (id=%u, size=%" PRISIZE_T ")", m_spawnGroupOverride, group.m_id, group.m_locations.size());
			}
			++ group.m_useCount;
			m_activeInitialSpawnGroupIndex = m_spawnGroupOverride;
			CHANGED_NETWORK_STATE(m_pGameRules, eEA_GameServerD);

			IGameRulesObjectivesModule *pObjectives = m_pGameRules->GetObjectivesModule();
			if (pObjectives)
			{
				pObjectives->OnNewGroupIdSelected(m_initialSpawnLocations[m_spawnGroupOverride].m_id);
			}

			// Disarm after use
			m_spawnGroupOverride = -1; 
			return;
		}

		CryFixedArray<int, 16> availableGroups;

		uint8 lowestUseCount = 0xFF;
		for (int i = 0; i < numGroups; ++ i)
		{
			if (i != m_activeInitialSpawnGroupIndex)
			{
				SSpawnGroup &group = m_initialSpawnLocations[i];
				if (group.m_useCount < lowestUseCount)
				{
					availableGroups.clear();
					availableGroups.push_back(i);
					lowestUseCount = group.m_useCount;
				}
				else if (group.m_useCount == lowestUseCount)
				{
					availableGroups.push_back(i);
				}
			}
		}

		int numAvailableGroups = availableGroups.size();
		if (numAvailableGroups > 0)
		{
			uint32 randomIndex = g_pGame->GetRandomNumber() % numAvailableGroups;
			uint32 groupIndexToUse = availableGroups[randomIndex];
			SSpawnGroup &group = m_initialSpawnLocations[groupIndexToUse];
			if (g_pGameCVars->g_debugSpawnPointsRegistration != 0)
			{
				CryLog("[SPAWN POINT] Activating spawn group %u (id=%u, size=%" PRISIZE_T ")", groupIndexToUse, group.m_id, group.m_locations.size());
			}
			++ group.m_useCount;
			m_activeInitialSpawnGroupIndex = groupIndexToUse;
			CHANGED_NETWORK_STATE(m_pGameRules, eEA_GameServerD);

			IGameRulesObjectivesModule *pObjectives = m_pGameRules->GetObjectivesModule();
			if (pObjectives)
			{
				pObjectives->OnNewGroupIdSelected(m_initialSpawnLocations[groupIndexToUse].m_id);
			}
		}
	}
}

void CGameRulesSpawningBase::SetInitialSpawnGroup( const char* groupName )
{
	// Gen crc, and find matching spawn group.
	uint32 spawnGroupId = CCrc32::ComputeLowercase(groupName);
	int numGroups = m_initialSpawnLocations.size();
	for (int i = 0; i < numGroups; ++ i)
	{
		if (m_initialSpawnLocations[i].m_id == spawnGroupId)
		{
			m_spawnGroupOverride = i; 
			break;
		}
	}
}
