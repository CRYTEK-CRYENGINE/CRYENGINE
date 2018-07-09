// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 

-------------------------------------------------------------------------
History:
- 03:03:2011 : Created by Richard Semmens

*************************************************************************/

#include "StdAfx.h"
#include "SpawningVisTable.h"
#include "GameRules.h"
#include "GameRulesMPSpawning.h"

#define SPAWNING_VIS_TABLE_DEBUG 0

#if SPAWNING_VIS_TABLE_DEBUG
#define SVT_Log CryLogAlways
#else
#define SVT_Log(...) (void)(0)
#endif

CSpawningVisTable::CSpawningVisTable()
{
	//Ensure that we have enough bits in the variable type that we are using to store the spawn visibility in
	static_assert(CSpawningVisTable::kMaxNumPlayers <= CSpawningVisTable::kNumBits, "Too many players!");

	m_spawnVisData.reserve(kInitialNumSpawns);
	m_spawnTeamList.reserve(kInitialNumSpawns);
	m_spawnVisTestList.reserve(kInitialNumSpawns / 3);
	m_spawnDependentList.reserve((kInitialNumSpawns * 2) / 3);
	m_spawnParentList.reserve(kInitialNumSpawns);	
	
	m_spawnPosnData.reserve(kInitialNumSpawns);
	m_playerPosnData.reserve(kMaxNumPlayers);

	m_bBlockPlayerAddition = false;
	m_nLastTestIndex = 0;
}

CSpawningVisTable::~CSpawningVisTable()
{
}

void CSpawningVisTable::Update(EntityId currentBestSpawnId)
{
	//Single loop or dual loop?
	if(m_playerList.size() > 0)
	{
		const int kNumLineTests = g_pGameCVars->g_spawn_vistable_numLineTestsPerFrame;
		const int kNumAreaTestsPerFrame = g_pGameCVars->g_spawn_vistable_numAreaTestsPerFrame;
		int nSkipSpawnIndex = -1;

		m_playerPosnData.clear();

		for(int i = 0, size = m_playerList.size(); i < size; i++)
		{
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(m_playerList[i]);
			if(pEntity == NULL)
			{
				SVT_Log("Missing entity for player! Player index %d, player EntityId %08X\n", i, m_playerList[i]);
			}
			Vec3 playerPos = pEntity != NULL ? pEntity->GetWorldPos() : ZERO;
			playerPos.z += 1.65f;
			m_playerPosnData.push_back(playerPos);
		}

		if(currentBestSpawnId)
		{
			TSpawnIndexMap::iterator bestSpawnIter = m_spawnIndexMap.find(currentBestSpawnId);
			assert(bestSpawnIter != m_spawnIndexMap.end());
			nSkipSpawnIndex = bestSpawnIter->second;

			SSpawnVisibilityInfo& rSpawnVisInfo = m_spawnVisData[nSkipSpawnIndex];

			//Only give it special treatment if it is a 'LineTest' type check,
			//	as all of the area checks will be tested against every frame (currently!)
			if(ShouldLineTestSpawn(rSpawnVisInfo))
			{
				SVT_Log("Spawn 0x%p is preferential", &rSpawnVisInfo);
				QueueNextLineTestForSpawn(rSpawnVisInfo, m_spawnPosnData[nSkipSpawnIndex]);		//Test the best spawn now, skip it later if required
			}
		}

		DoVisibilityTests(kNumLineTests, kNumAreaTestsPerFrame, nSkipSpawnIndex);

		UpdateDependents();
	}
}

void CSpawningVisTable::DoVisibilityTests(const int kMaxNumLineTests, const int kMaxNumAreaTests, const int nSkipSpawnIndex)
{
	int	nNumLinetests = 0, nNumAreaTests = 0, size = m_spawnVisTestList.size();
	const int start = m_nLastTestIndex >= size ? 0 : m_nLastTestIndex;
	
	for(int i = start, iter = 0; iter < size; iter++)
	{
		const int nSpawnIndex = m_spawnVisTestList[i];
		SSpawnVisibilityInfo& rSpawnVisInfo = m_spawnVisData[nSpawnIndex];

		if(ShouldLineTestSpawn(rSpawnVisInfo) && nNumLinetests < kMaxNumLineTests)
		{
			SVT_Log("IDX: %03d ShouldLineTestSpawn passed", nSpawnIndex);
			QueueNextLineTestForSpawn(rSpawnVisInfo, m_spawnPosnData[nSpawnIndex]);
			m_nLastTestIndex = i;
			nNumLinetests++;
		}
		else
		{
			ESpawnTestType tt = rSpawnVisInfo.spawnTestType;
			if(tt == eSTT_AreaTest)
			{
				SVT_Log("IDX: %03d Area Test", nSpawnIndex);
				PerformAreaTestForSpawn(rSpawnVisInfo);
				nNumAreaTests++;
				m_updatedSpawns.push_back(nSpawnIndex);
			}
		}

		i = (i + 1 >= size) ? 0 : i + 1;
	}
}

// copied from tunneling grenade
static uint32 PROBE_TYPES = ent_terrain|ent_static;
static uint32 PROBE_FLAGS = (geom_colltype_ray)<<rwi_colltype_bit|rwi_colltype_any|rwi_ignore_noncolliding|rwi_ignore_solid_back_faces|8;

void CSpawningVisTable::QueueNextLineTestForSpawn(SSpawnVisibilityInfo& rSpawnVisInfo, const Vec3& rSpawnPosn)
{
	const int kPlayerIndexToTest = rSpawnVisInfo.currentlyTestingVis + 1 >= (int)m_playerList.size() ? 0 : rSpawnVisInfo.currentlyTestingVis + 1;
	rSpawnVisInfo.currentlyTestingVis = kPlayerIndexToTest;

	const Vec3& playerPos = m_playerPosnData[kPlayerIndexToTest];
	Vec3 dir = playerPos - rSpawnPosn;

	SVT_Log("  QueueNextLineTestForSpawn Spawn: 0x%p   PlayerIndex: %d", &rSpawnVisInfo, kPlayerIndexToTest);
	
	rSpawnVisInfo.rayId = g_pGame->GetRayCaster().Queue(
															RayCastRequest::HighPriority, 
															RayCastRequest(rSpawnPosn, dir, PROBE_TYPES, PROBE_FLAGS, NULL, 0),
															functor(rSpawnVisInfo, &SSpawnVisibilityInfo::OnRayCastDataReceived));
}

void CSpawningVisTable::PerformAreaTestForSpawn(SSpawnVisibilityInfo& rSpawnVisInfo)
{
	IEntitySystem * pEntitySystem = gEnv->pEntitySystem;

	const int kPlayerIndexToTest = rSpawnVisInfo.currentlyTestingVis + 1 >= (int)m_playerList.size() ? 0 : rSpawnVisInfo.currentlyTestingVis + 1;	
	rSpawnVisInfo.currentlyTestingVis = kPlayerIndexToTest;

	const Vec3& playerPos = m_playerPosnData[kPlayerIndexToTest];
	const uint32 playerBit = (0x1 << kPlayerIndexToTest);	//Slow on consoles. Not much we can do.
	
	IEntity * pSpawn				= pEntitySystem->GetEntity(rSpawnVisInfo.entityId);
	IEntityLink *pEntityLink = pSpawn->GetEntityLinks();

	SVT_Log("  PerformAreaTestForSpawn: 0x%p   PlayerIndex: %d", &rSpawnVisInfo, kPlayerIndexToTest);

	rSpawnVisInfo.visBits &= ~playerBit;

	for (IEntityLink *pLink = pEntityLink; pLink; pLink = pLink->next)
	{
		IEntity* pLinkedEntity = pEntitySystem->GetEntity(pLink->entityId);
		IEntityAreaComponent *pArea = (IEntityAreaComponent*)pLinkedEntity->GetProxy(ENTITY_PROXY_AREA);
		if (pArea->CalcPointWithin(INVALID_ENTITYID, playerPos, false))
		{
			rSpawnVisInfo.visBits |= playerBit;
			break;
		}
	}
}

void CSpawningVisTable::SSpawnVisibilityInfo::OnRayCastDataReceived( const QueuedRayID& rRayId, const RayCastResult& result )
{
	if(rayId != rRayId)
		CryFatalError("BROKEN! The ray Id on the callback is not the ray ID in the vis info");

	SVT_Log("OnRayCastDataReceived Spawn: 0x%p   PlayerIndex: %d", this, currentlyTestingVis);
	const uint32 playerBit = (0x1 << currentlyTestingVis);	//Slow on consoles. Not much we can do.	
	
	if (result.hitCount)
	{
		visBits &= ~playerBit;
	}
	else
	{
		visBits |= playerBit;
	}

	//Mark as able to do the next test
	rayId = 0;
}

void CSpawningVisTable::SSpawnVisibilityInfo::CancelRaycastRequest()
{
	g_pGame->GetRayCaster().Cancel(rayId);
	rayId = 0;
}

void CSpawningVisTable::UpdateDependents()
{
	//Iterate over m_updatedSpawns and update the dependent spawns' 'visibility'
	for(int i = 0, size1 = m_spawnDependentList.size(); i < size1; i++)
	{
		const int nParentIndex = m_spawnParentList[i];
		for(int j = 0, size2 = m_updatedSpawns.size(); j < size2; j++)
		{
			const int nUpdatedSpawnIndex = m_updatedSpawns[j];
			if( nUpdatedSpawnIndex == nParentIndex )
			{
				m_spawnVisData[m_spawnDependentList[i]].visBits = m_spawnVisData[nUpdatedSpawnIndex].visBits;
			}
		}
	}	
}

bool CSpawningVisTable::IsSpawnLocationVisibleByTeam(EntityId location, int teamId) const
{
#if defined(DEDICATED_SERVER)
	CryFatalError("IsSpawnLocationVisibleBy should not be called on the dedicated server!");
#endif

	//We don't need to check the teamId unless we're running the vis table on the dedicated server
	//	they're asking about this for the enemy team of the local player. And that's all we're tracking
	TSpawnIndexMap::const_iterator iter = m_spawnIndexMap.find(location);
	assert(iter != m_spawnIndexMap.end());

	const SSpawnVisibilityInfo& rSpawnInfo = m_spawnVisData[iter->second];

	//Should we be delaying the update of the visibility of dependent spawns until evaluation
	//	at this point, or should we be updating every frame? Currently every frame.
	return (rSpawnInfo.visBits != 0);
}

void CSpawningVisTable::AddSpawnLocation(EntityId location, bool doVisibilityTest)
{
	//This spawn should not already be present
	assert(m_spawnIndexMap.end() == m_spawnIndexMap.find(location));

	const int kSpawnIdx = m_spawnVisData.size();
	SSpawnVisibilityInfo spawnVisInfo(location);

	IEntity * pSpawnEntity = gEnv->pEntitySystem->GetEntity(location);
	CGameRulesMPSpawningBase * pGameRulesSpawningModule = static_cast<CGameRulesMPSpawningBase*>(g_pGame->GetGameRules()->GetSpawningModule());

	Vec3 vSpawnPosn = pSpawnEntity->GetWorldPos();
	vSpawnPosn.z += 1.5f;	//Increase the height of the spawner test posn to approximate head height

	m_spawnVisData.push_back(spawnVisInfo);
	m_spawnPosnData.push_back(vSpawnPosn);
	m_spawnTeamList.push_back(pGameRulesSpawningModule->GetSpawnLocationTeam(location));
	
	if(doVisibilityTest)
		m_spawnVisTestList.push_back(kSpawnIdx);
	else
		m_spawnDependentList.push_back(kSpawnIdx);

	m_spawnIndexMap.insert(std::make_pair(location, kSpawnIdx));
}

//This function should only be being called from the editor, as far as I know
void CSpawningVisTable::RemoveSpawnLocation(EntityId location)
{
	TSpawnIndexMap::iterator spawnIdxIter = m_spawnIndexMap.find(location);
	if (spawnIdxIter == m_spawnIndexMap.end())
	{
		return;
	}

	int spawnIdx			= spawnIdxIter->second;
	TSpawnIndexMap::reverse_iterator finalSpawnIdxIter = m_spawnIndexMap.rbegin();
	int finalSpawnIdx = finalSpawnIdxIter->second;
	
	if(m_spawnVisData[spawnIdx].rayId)
	{
		m_spawnVisData[spawnIdx].CancelRaycastRequest();
	}

	if(m_spawnVisData[finalSpawnIdx].rayId)
	{
		m_spawnVisData[finalSpawnIdx].CancelRaycastRequest();
	}

	//Update the map of entity Ids to spawn indices for the moved spawn data
	finalSpawnIdxIter->second = spawnIdx;

	m_spawnVisData[spawnIdx]	= m_spawnVisData[finalSpawnIdx];
	m_spawnTeamList[spawnIdx]	= m_spawnTeamList[finalSpawnIdx];
	m_spawnPosnData[spawnIdx]	= m_spawnPosnData[finalSpawnIdx];

	bool bFound = false;
	for(int i = 0, size = m_spawnVisTestList.size(); i < size; i++)
	{
		if(m_spawnVisTestList[i] == spawnIdx)
		{
			bFound = true;
			m_spawnVisTestList.erase(m_spawnVisTestList.begin() + i);
			break;
		}
	}

	if(!bFound)
	{
		for(int i = 0, size = m_spawnDependentList.size(); i < size; i++)
		{
			if(m_spawnDependentList[i] == spawnIdx)
			{
				m_spawnDependentList.erase(m_spawnDependentList.begin() + i);
				break;
			}
		}
	}	

	m_spawnIndexMap.erase(spawnIdxIter);
}

void CSpawningVisTable::Initialise()
{	
	IEntitySystem * pEntitySystem = gEnv->pEntitySystem;
	const int kNumSpawns = m_spawnVisData.size();
	const IEntityClassRegistry * pClassRegistry = pEntitySystem->GetClassRegistry();

	const IEntityClass * pAreaBoxClass			= pClassRegistry->FindClass("AreaBox");
	const IEntityClass * pAreaSphereClass		= pClassRegistry->FindClass("AreaSphere");

	for(int i = 0; i < kNumSpawns; i++)
	{
		SSpawnVisibilityInfo& rSpawnVisInfo = m_spawnVisData[i];

		IEntity * pSpawner = pEntitySystem->GetEntity(rSpawnVisInfo.entityId);
		IEntityLink *pEntityLink = pSpawner->GetEntityLinks();

		for (IEntityLink *pLink = pEntityLink; pLink; pLink = pLink->next)
		{
			IEntity* pLinkedEntity = gEnv->pEntitySystem->GetEntity(pLink->entityId);
			const IEntityClass * pEntityClass = pLinkedEntity->GetClass();
			if ( pEntityClass == pAreaBoxClass || pEntityClass == pAreaSphereClass )
			{
				rSpawnVisInfo.spawnTestType = eSTT_AreaTest;
				IEntityAreaComponent *pArea = (IEntityAreaComponent*)pLinkedEntity->GetProxy(ENTITY_PROXY_AREA);
				if(!pArea)
					CryFatalError("No ENTITY_PROXY_AREA for entity that is of AreaBox or AreaSphere class!");
			}
			else
			{
				CryFatalError("Linked entity that isn't an AreaBox or AreaSphere!");
				rSpawnVisInfo.spawnTestType = eSTT_LineTest;
				break;
			}
		}
	}

	//Iterate through the dependent spawns and link up with spawns that are doing the tests
	for(int i = 0, size1 = m_spawnDependentList.size(); i < size1; i++)
	{
		const int kDependentSpawnIndex	= m_spawnDependentList[i];
		const Vec3& dependentSpawnPos		= m_spawnPosnData[kDependentSpawnIndex];

		int nClosestIndex = -1;
		float fClosestSpawnDistSq = FLT_MAX;
		for(int j = 0, size2 = m_spawnVisTestList.size(); j < size2; j++)
		{
			const int nPotentialParentIndex = m_spawnVisTestList[j];
			const float fDistanceSq = dependentSpawnPos.GetSquaredDistance(m_spawnPosnData[nPotentialParentIndex]);
			if(fDistanceSq < fClosestSpawnDistSq)
			{
				nClosestIndex = nPotentialParentIndex;
				fClosestSpawnDistSq = fDistanceSq;
			}
		}

		if(nClosestIndex == -1)
		{
			if (!gEnv->IsEditor())
			{
				CryFatalError("Failed to find any testing spawns in the level. Needs fixing. Now.");
			}
		}
		else
		{
			m_spawnParentList.push_back(nClosestIndex);
		}
	}
}


void CSpawningVisTable::PlayerJoined(EntityId newPlayerId)
{
	SVT_Log("PlayerJoined with EntityId %08X\n", newPlayerId);
	CGameRules * pGameRules = g_pGame->GetGameRules();
	if(pGameRules->GetTeamCount() == 0)
		OnSetTeam(newPlayerId, 0);
}

void CSpawningVisTable::OnSetTeam(EntityId playerId, int teamId)
{
	const EntityId localPlayerId = g_pGame->GetIGameFramework()->GetClientActorId();
#if !defined(DEDICATED_SERVER)
	if(localPlayerId)
#endif
	{
		if(!m_bBlockPlayerAddition)
		{
			if(playerId != localPlayerId)
			{
				int nIndex = -1;
				for(int i = 0, size = m_playerList.size(); i < size; i++)
				{
					if(m_playerList[i] == playerId)
					{
						nIndex = i;
						break;
					}
				}

				CGameRules * pGameRules = g_pGame->GetGameRules();
				int nLocalPlayerTeamId	= pGameRules->GetTeam(localPlayerId);

				//Ensure it's not already in the list
				if(nIndex == -1)
				{					
					if(teamId != nLocalPlayerTeamId)
					{
						SVT_Log("CSpawningVisTable:PlayerJoined() - adding player EntityId %d", playerId);		
						m_playerList.push_back(playerId);
						IEntity * pEntity = gEnv->pEntitySystem->GetEntity(playerId);
						m_playerPosnData.push_back(pEntity->GetWorldPos());
						SVT_Log("Player ID %08X set team, not in list, ADDING as the team is NOT the same as the local players'\n", playerId);
					}
					else
					{
						SVT_Log("Player ID %08X set team, not in list, not adding as the team is the same as the local players'\n", playerId);
					}
				}
				else if(teamId == nLocalPlayerTeamId)	
				{
					SVT_Log("Player ID %08X set team, found at index %d calling PlayerLeft as the team is the same as the local players'\n", playerId, nIndex);
					//Remove the player from the list as they're on the same side as the local player
					PlayerLeft(playerId);
				}
			}
			else
			{
				RepopulatePlayerList();
			}
		}
		else
		{
			SVT_Log("CSpawningVisTable:PlayerJoined() - aborted adding player %d due to bBlockPlayerAddition flag set", playerId);
		}		
	}
}

void CSpawningVisTable::PlayerLeft(EntityId playerId)
{
	int i;
	const int kNumPlayers = m_playerList.size();
	for(i = 0; i < kNumPlayers; i++)
	{
		if(m_playerList[i] == playerId)
			break;
	}

	if(i != kNumPlayers)
	{
		const int kFinalIndex = kNumPlayers - 1;
		SVT_Log("Player ID %08X left, found at index %d, swapping objects from index %d and removing last index\n", playerId, i, kFinalIndex);
		m_playerList[i] = m_playerList[kFinalIndex];
		m_playerPosnData[i] = m_playerPosnData[kFinalIndex];
		m_playerList.pop_back();
		m_playerPosnData.pop_back();
		const int kBitToClear = ~(0x1 << kFinalIndex);

		for(int j = 0, size = m_spawnVisData.size(); j < size; j++)
		{
			SSpawnVisibilityInfo& rSpawnVisInfo = m_spawnVisData[j];
			rSpawnVisInfo.visBits &= kBitToClear;
			if(rSpawnVisInfo.rayId != 0)
			{
				if( rSpawnVisInfo.currentlyTestingVis == i )
				{
					rSpawnVisInfo.CancelRaycastRequest();
				}				
			}
		}
	}
}

void CSpawningVisTable::HostMigrationResumeAddingPlayers()
{
	m_bBlockPlayerAddition = false;
	
	RepopulatePlayerList();

	//TODO:
	//	Clear current testing data
}

void CSpawningVisTable::RepopulatePlayerList()
{
	//	Clear the list of actors and re-add them
	SVT_Log(" >> Complete Repopulation of player list due to Local Player switching teams, or host migration finishing << \n");
	m_playerList.clear();

	IActorIteratorPtr it		= g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
	EntityId localEntityId	= g_pGame->GetIGameFramework()->GetClientActorId();
	CGameRules * pGameRules = g_pGame->GetGameRules();

	while (IActor *pActor = it->Next())
	{
		EntityId entityId = pActor->GetEntityId();
		if(entityId != localEntityId)
			OnSetTeam(entityId, pGameRules->GetTeam(entityId));
	}
}