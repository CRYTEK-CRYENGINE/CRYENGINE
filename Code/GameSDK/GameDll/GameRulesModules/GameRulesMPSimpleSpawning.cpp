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
#include "GameRulesMPSimpleSpawning.h"
#include "GameRules.h"

#include "ActorManager.h"

////////////////////////////////////////////////////////
// Rich S SPAWNING
////////////////////////////////////////////////////////

CGameRulesRSSpawning::CGameRulesRSSpawning()
{
	m_initialized = false;
}

CGameRulesRSSpawning::~CGameRulesRSSpawning()
{

}

EntityId CGameRulesRSSpawning::GetSpawnLocationTeamGame(EntityId playerId, const Vec3 &deathPos)
{
	if(!m_initialized)
	{
		UpdateMapCentre();
		
		//TODO: Fix, move to a proper init call instead of lazy initialization. Game rules init?
		m_spawningVisTable.Initialise();

		m_initialized = true;
	}

	Vec3 enemyTeamCentre;
	Vec3 friendlyTeamCentre;
	Vec3 idealSpawnLocation;

	SpawnLogAlways(">>> Using RS Spawning <<<");

	SUsefulSpawnData spawnData;

	PopulateSpawnData(spawnData, playerId);

	if(spawnData.numActiveEnemyPlayers + spawnData.numActiveFriendlyPlayers > 0)
	{
		if(spawnData.numActiveFriendlyPlayers == 0)
		{
			SpawnLogAlways("[SPAWN] Only ENEMY players present");
			//There are only friendlies alive in the game
			GetEnemyTeamCentre(spawnData, &enemyTeamCentre);

			SpawnLogAlways("[SPAWN] > Enemy Team Centre calculated as (%.2f, %.2f, %.2f)", enemyTeamCentre.x, enemyTeamCentre.y, enemyTeamCentre.z);

			GetIdealSpawnLocation(spawnData, &enemyTeamCentre, &m_mapCentre, &idealSpawnLocation);
		}
		else if(spawnData.numActiveEnemyPlayers == 0)
		{
			SpawnLogAlways("[SPAWN] Only FRIENDLY players present");
			//There are only enemies alive in the game
			GetFriendlyTeamCentre(spawnData, &friendlyTeamCentre);

			GetIdealSpawnLocation(spawnData, &m_mapCentre, &friendlyTeamCentre, &idealSpawnLocation);
		}
		else
		{
			SpawnLogAlways("[SPAWN] Normal spawn");

			//Both friendly and enemy players are present. Do the full spawn code
			GetEnemyTeamCentre(spawnData, &enemyTeamCentre);

			GetFriendlyTeamCentre(spawnData, &friendlyTeamCentre);

			GetIdealSpawnLocation(spawnData, &enemyTeamCentre, &friendlyTeamCentre, &idealSpawnLocation);
		}

		return GetBestSpawnUsingWeighting(spawnData, &idealSpawnLocation);
	}
	else
	{
		SpawnLogAlways("[SPAWN] Random spawn occurring");

		TSpawnLocations& spawnLocations = GetSpawnLocations(playerId);

		//There is no-one in the game, use a random spawner. 
		return GetRandomSpawnLocation_Cached(spawnLocations, playerId, spawnData.playerTeamId, spawnData.isInitialSpawn);
	}	
}

void CGameRulesRSSpawning::PopulateSpawnData(SUsefulSpawnData& spawnData, EntityId playerId)
{

	int playerTeamId = m_pGameRules->GetTeam(playerId);
	int enemyTeamId	 = m_pGameRules->GetEnemyTeamId(playerTeamId);

	spawnData.playerId			= playerId;	

	spawnData.playerTeamId	= playerTeamId;

	spawnData.enemyTeamId		= enemyTeamId;

	
	//Find out how many enemy players are active
	int numActiveEnemyPlayers = 0;
	EntityId enemyPlayerId = 0;
	while( enemyPlayerId = m_pGameRules->GetTeamActivePlayer(enemyTeamId, numActiveEnemyPlayers++));

	//numActiveEnemyPlayers gets incremented after the call regardless of whether a player was found or not
	spawnData.numActiveEnemyPlayers = numActiveEnemyPlayers - 1;

	//Find out how many friendly players are active
	int numActiveFriendlyPlayers = 0;
	EntityId friendlyPlayerId = 0;
	while( friendlyPlayerId = m_pGameRules->GetTeamActivePlayer(playerTeamId, numActiveFriendlyPlayers++));

	//numActiveFriendlyPlayers gets incremented after the call regardless of whether a player was found or not
	spawnData.numActiveFriendlyPlayers = numActiveFriendlyPlayers - 1;

	spawnData.isInitialSpawn = IsPlayerInitialSpawning(playerId);
}

void CGameRulesRSSpawning::GetEnemyTeamCentre( SUsefulSpawnData& spawnData, Vec3 * pOutCentre )
{
	//This is now a blueprint for switching over the other functions to use the CActorManager.
	//	They are not being switched over now because there are more significant changes in MPTrunk
	//	that further changes would conflict with - Rich S

	int				idx						= 0;
	EntityId	enemyPlayerId = 0;

	float fNumEnemies				= 0.0f;
	Vec3	pureAveragePosition(0.0f, 0.0f, 0.0f);

	CActorManager * pActorManager = CActorManager::GetActorManager();
	
	pActorManager->PrepareForIteration();

	const int kNumActors = pActorManager->GetNumActors();

	for(int i = 0; i < kNumActors; i++)
	{
		SActorData actorData;
		pActorManager->GetNthActorData(i, actorData);

		if(actorData.teamId == spawnData.enemyTeamId && actorData.spectatorMode == CActor::eASM_None)
		{
			fNumEnemies += 1.0f;

			float fInvNumEnemies						= __fres(fNumEnemies);
			float fCurrentPositionWeighting = 1.0f - fInvNumEnemies;

			//This is slightly more computationally expensive than adding them all up, but 
			//	avoids the potential for some float precision problems
			pureAveragePosition = (pureAveragePosition * fCurrentPositionWeighting) + (fInvNumEnemies * actorData.position);
		}		
	}

	*pOutCentre = pureAveragePosition;
}


////////////////////////////////////////////////
// This is currently identical to the GetEnemyTeamCentre function,
//	but functionality will change in the future
void CGameRulesRSSpawning::GetFriendlyTeamCentre( SUsefulSpawnData& spawnData, Vec3 * pOutCentre )
{
	int				idx								= 0;
	EntityId	friendlyPlayerId	= 0;

	float fNumFriendlies				= 0.0f;
	Vec3	pureAveragePosition(0.0f, 0.0f, 0.0f);

	while( friendlyPlayerId = m_pGameRules->GetTeamActivePlayer(spawnData.playerTeamId, idx++))
	{
		fNumFriendlies += 1.0f;

		const IEntity * pFriendlyPlayer = gEnv->pEntitySystem->GetEntity(friendlyPlayerId);

		Vec3 friendlyPosition = pFriendlyPlayer->GetPos();

		float fInvNumFriendlies					= __fres(fNumFriendlies);
		float fCurrentPositionWeighting = 1.0f - fInvNumFriendlies;

		//This is slightly more computationally expensive than adding them all up, but 
		//	avoids the potential for some float precision problems
		pureAveragePosition = (pureAveragePosition * fCurrentPositionWeighting) + (fInvNumFriendlies * friendlyPosition);
	}

	*pOutCentre = pureAveragePosition;
}

void CGameRulesRSSpawning::GetIdealSpawnLocation( SUsefulSpawnData& spawnData, const Vec3 * pEnemyCentre, const Vec3 * pFriendlyCentre, Vec3 * pOutIdealSpawnLoc )
{
	Vec3 enemyCentre			= *pEnemyCentre;
	Vec3 friendlyCentre		= *pFriendlyCentre;
	
	Vec3 enemyToFriendly = friendlyCentre - enemyCentre;
	float fEnemyToFriendlyDistanceSq = enemyToFriendly.len2();
	
	Vec3 dirEnemyToFriendly;
	
	if(fEnemyToFriendlyDistanceSq > 3.0f)
	{
		float fInvEnemyToFriendlyDistance = isqrt_fast_tpl(fEnemyToFriendlyDistanceSq);

		dirEnemyToFriendly = enemyToFriendly * fInvEnemyToFriendlyDistance;
	}
	else
	{
		//Tend the two teams towards opposing sides of the map if the centres are too close together
		if(spawnData.enemyTeamId > spawnData.playerTeamId)
		{
			dirEnemyToFriendly.Set(0.0f,  1.0f, 0.0f);
		}
		else
		{
			dirEnemyToFriendly.Set(0.0f, -1.0f, 0.0f);
		}		
	}

	const float fDistanceBehindFriendliesToSpawn = 35.0f;

	*pOutIdealSpawnLoc = friendlyCentre + (fDistanceBehindFriendliesToSpawn * dirEnemyToFriendly);
}

EntityId CGameRulesRSSpawning::GetClosestSpawnToLocation( SUsefulSpawnData& spawnData, const Vec3 * pLocation )
{
	EntityId bestSpawnId = 0;
	Vec3 idealLocation = *pLocation;
	float fClosestDistSq = FLT_MAX;

	const TSpawnLocations& spawnLocations = GetSpawnLocations(spawnData.playerId);

	for (TSpawnLocations::const_iterator it=spawnLocations.begin(); it!=spawnLocations.end(); ++it)
	{
		EntityId spawnId(*it);
		const IEntity *pSpawn = gEnv->pEntitySystem->GetEntity(spawnId);

		Vec3 spawnPosition = pSpawn->GetPos();

		Vec3 idealToSpawnPosition = spawnPosition - idealLocation;

		float fDistanceFromIdealSq = idealToSpawnPosition.len2();

		if(fDistanceFromIdealSq < fClosestDistSq)
		{
			fClosestDistSq = fDistanceFromIdealSq;
			bestSpawnId = spawnId;
		}
	}

	return bestSpawnId;
}

void CGameRulesRSSpawning::UpdateMapCentre()
{
	float fNumSpawns = 0.0f;
	Vec3 averagePos(0.0f, 0.0f, 0.0f);

	TSpawnLocations::const_iterator itEnd = m_spawnLocations.end();
	for (TSpawnLocations::const_iterator it=m_spawnLocations.begin(); it!=itEnd; ++it)
	{
		UpdateSpawnPointAverage((*it), fNumSpawns, averagePos);
	}

	int numInitialSpawnGroups = m_initialSpawnLocations.size();
	for (int i = 0; i < numInitialSpawnGroups; ++ i)
	{
		TSpawnLocations &spawnPoints = m_initialSpawnLocations[i].m_locations;
		itEnd = spawnPoints.end();
		for (TSpawnLocations::const_iterator it = spawnPoints.begin(); it!=itEnd; ++it)
		{
			UpdateSpawnPointAverage((*it), fNumSpawns, averagePos);
		}
	}

	SpawnLogAlways("[SPAWN] Map centre calculated as (%.2f, %.2f, %.2f) from average of spawners", averagePos.x, averagePos.y, averagePos.z);

	m_mapCentre = averagePos;
}

EntityId CGameRulesRSSpawning::GetBestSpawnUsingWeighting( SUsefulSpawnData& spawnData, const Vec3 * pBestLocation )
{
	float fBestScore = FLT_MAX;
	EntityId bestSpawnerId = 0;
	const Vec3 idealLocation = *pBestLocation;

	TPositionList FriendlyPositions, EnemyPositions;
	TMultiplierList FriendlyScoreMultipliers;
	FriendlyPositions.reserve(spawnData.numActiveFriendlyPlayers);
	FriendlyScoreMultipliers.reserve(spawnData.numActiveFriendlyPlayers);
	EnemyPositions.reserve(spawnData.numActiveEnemyPlayers);

	UpdateFriendlyPlayerPositionsAndMultipliers(spawnData, FriendlyPositions, FriendlyScoreMultipliers);
	UpdateEnemyPlayerPositions(spawnData, EnemyPositions);

	SpawnLogAlways("[SPAWN] Ideal location calculated as (%.2f, %.2f, %.2f)", idealLocation.x, idealLocation.y, idealLocation.z);

	int iSpawnIndex = 0;

#if MONITOR_BAD_SPAWNS
	m_DBG_spawnData.scoringResults.clear();
#endif

	EntityId lastSpawnId = 0;
	Vec3 lastSpawnPos(ZERO);
	{
		TPlayerDataMap::iterator it = m_playerValues.find(spawnData.playerId);
		if (it != m_playerValues.end())
		{
			lastSpawnId		= it->second.lastSpawnLocationId;

			if(lastSpawnId)
			{
				IEntity * pLastSpawnEnt = gEnv->pEntitySystem->GetEntity(lastSpawnId);
				lastSpawnPos	= pLastSpawnEnt->GetWorldPos();
			}
		}
	}

	EntityId lastPrecachedSpawnId = 0;
	Vec3 lastPrecachedSpawnPos(ZERO);
	if(spawnData.playerId==gEnv->pGameFramework->GetClientActorId())
	{
		if(IEntity* pLastPrecachedSpawnEnt = gEnv->pEntitySystem->GetEntity(m_currentBestSpawnId))
		{
			lastPrecachedSpawnId = m_currentBestSpawnId;
			lastPrecachedSpawnPos = pLastPrecachedSpawnEnt->GetWorldPos();
		}
	}

	TSpawnLocations& spawnLocations = GetSpawnLocations(spawnData.playerId);

#if !defined(_RELEASE)
	int nWrongTeamSpawns = 0, nUnsafeSpawns = 0;
#endif

	bool bCheckLineOfSight = !gEnv->IsDedicated();
	for (TSpawnLocations::const_iterator it=spawnLocations.begin(); it!=spawnLocations.end(); ++it)
	{
		SpawnLogAlways("[SPAWN] Spawn #%d", iSpawnIndex);

		EntityId spawnId(*it);

		int spawnTeam=GetSpawnLocationTeam(spawnId);
		if ((spawnTeam == 0) || (spawnTeam == spawnData.playerTeamId))
		{
			const IEntity *pSpawn = gEnv->pEntitySystem->GetEntity(spawnId);

			Vec3 spawnPosition = pSpawn->GetPos();

			const float fScoreFromEnemies			= GetScoreFromProximityToEnemies(spawnData, EnemyPositions, spawnPosition);
			const float fScoreFromFriendlies	= GetScoreFromProximityToFriendlies(spawnData, FriendlyPositions, FriendlyScoreMultipliers, spawnPosition);
			const float fScoreFromIdealPosn		=	GetScoreFromProximityToIdealLocation(spawnData, idealLocation, spawnPosition);
			const float fScoreFromExclusions	= GetScoreFromProximityToExclusionZones(spawnData, spawnId);
			const float fScoreFromLineOfSight	=	bCheckLineOfSight ? GetScoreFromLineOfSight(spawnData, spawnId) : 0.f;
			float fScoreFromLastSpawn		= 0.0f;
			
			if(lastSpawnId != 0)
				fScoreFromLastSpawn = GetScoreFromProximityToPreviousSpawn(lastSpawnPos, spawnPosition);
			
			float fScoreFromLastSelectedSpawn = 0.0f;
			if(lastPrecachedSpawnId)
				fScoreFromLastSelectedSpawn = GetScoreFromProximityToLastPrecachedSpawn(lastPrecachedSpawnPos, spawnPosition);

			const float fScore = fScoreFromEnemies + fScoreFromFriendlies + fScoreFromIdealPosn + fScoreFromExclusions + fScoreFromLineOfSight + fScoreFromLastSpawn;

#if MONITOR_BAD_SPAWNS
			m_DBG_spawnData.scoringResults.push_back(SScoringResults(fScoreFromEnemies, fScoreFromFriendlies, fScoreFromIdealPosn, fScoreFromLineOfSight > 0, fScoreFromExclusions > 0));
#endif

			if(fScore < fBestScore)
			{
				if(IsSpawnLocationSafe(spawnData.playerId, pSpawn, 1.0f, false, 0.0f))
				{
					SpawnLogAlways("[SPAWN] >> Total score %.6f, better than previous best of %.6f", fScore, fBestScore);
					bestSpawnerId = spawnId;
					fBestScore		= fScore;
#if MONITOR_BAD_SPAWNS
					m_DBG_spawnData.iSelectedSpawnIndex = iSpawnIndex;
					m_DBG_spawnData.selectedSpawn				= spawnId;
#endif
				}
				else
				{
#if !defined(_RELEASE)
					nUnsafeSpawns++;
#endif
					SpawnLogAlways("[SPAWN] >> Total score %.6f, better than %.6f but UNSAFE", fScore, fBestScore);
				}		
			}
			else
			{
				SpawnLogAlways("[SPAWN] >> Total score %.6f, too high, not using", fScore);
			}
		}
		else
		{
#if !defined(_RELEASE)
			nWrongTeamSpawns++;
#endif
		}
		
		iSpawnIndex++;
	}

#if !defined(_RELEASE)
	if(bestSpawnerId == 0)
	{
		IEntity * pPlayer = gEnv->pEntitySystem->GetEntity(spawnData.playerId);
		CryFatalError("ERROR: No spawner found. Should ALWAYS find a spawn.\n   Trying to spawn player EID %d, name '%s'.\n   Player is %s spawning.\n   %" PRISIZE_T " spawns found for spawn type, %d wrong team, %d unsafe", spawnData.playerId, pPlayer->GetName(), IsPlayerInitialSpawning(spawnData.playerId) ? "initial" : "normal", spawnLocations.size(), nWrongTeamSpawns, nUnsafeSpawns);
	}
#endif

	return bestSpawnerId;
}

float CGameRulesRSSpawning::GetScoreFromProximityToLastPrecachedSpawn( const Vec3& lastPrecachedPos, const Vec3& spawnPos ) const
{
	static const float kHighDist = 30.f;
	static const float kLowDist = 10.f;
	static const float kRange = kHighDist-kLowDist;
	static const float kRangeInv = 1.0f/kRange;
	static const float kScoreForBeingFarFromLastPrecached = 3.0f;

	const float fDist = (lastPrecachedPos-spawnPos).GetLengthFast();
	return clamp_tpl<float>(fDist-kLowDist, 0.f, kRange) * kRangeInv * kScoreForBeingFarFromLastPrecached;
}

float CGameRulesRSSpawning::GetScoreFromProximityToPreviousSpawn(const Vec3& lastSpawnPos, const Vec3& spawnPos) const
{
	const static float kPreviewSpawnProximityScore = 0.75f * fScoreForSpawnerBeingVisible;
	const float fDistSq = lastSpawnPos.GetSquaredDistance(spawnPos);
	return max(kPreviewSpawnProximityScore - fDistSq, 0.0f);
}

float CGameRulesRSSpawning::GetScoreFromProximityToEnemies( SUsefulSpawnData& spawnData, TPositionList& EnemyPlayerPositions, const Vec3& potentialSpawnPosition )
{
	SpawnLogAlways("[SPAWN] > GetScoreFromProximityToEnemies()");
	float fScoreFromEnemies = 0.0f;

	if(spawnData.numActiveEnemyPlayers > 0)
	{
		const float fScoreOnTopOfEnemy = 200.0f;
		const float fMultiplierForEnemyDistance = 1.7f;
		const float fDistanceScoreMultiplier = 200.0f;

		for(int i = EnemyPlayerPositions.size() - 1; i >= 0; i--)
		{
			Vec3  vDiffToEnemy			 = (EnemyPlayerPositions[i] - potentialSpawnPosition);
			vDiffToEnemy.z += max(2.0f * vDiffToEnemy.z, 6.0f);
			float fDistanceFromEnemy = vDiffToEnemy.len();

			float fDistanceScoreFraction = __fres(max(fDistanceFromEnemy - 4.f, 0.001f));

			float fScoreForThisEnemy = max(fScoreOnTopOfEnemy - (fDistanceFromEnemy * fMultiplierForEnemyDistance), 0.0f) + (fDistanceScoreFraction * fDistanceScoreMultiplier);

			SpawnLogAlways("[SPAWN] >>> Score from Enemy %d is %.6f at distance %.1f", i, fScoreForThisEnemy, fDistanceFromEnemy);

			fScoreFromEnemies = max(fScoreForThisEnemy, fScoreFromEnemies);
		}

		SpawnLogAlways("[SPAWN] >> Total Score from Enemies: %.6f", fScoreFromEnemies);
	}

	return fScoreFromEnemies;
}

float CGameRulesRSSpawning::GetScoreFromProximityToEnemiesNoTeams( SUsefulSpawnData& spawnData, const Vec3& potentialSpawnPosition )
{
	SpawnLogAlways("[SPAWN] > GetScoreFromProximityToEnemiesNoTeams()");
	float fScoreFromEnemies = 0.0f;

	CGameRules::TPlayers players;
	g_pGame->GetGameRules()->GetPlayers(players);

	if(players.size() > 1)
	{
		const float fScoreOnTopOfEnemy = 200.0f;
		const float fMultiplierForEnemyDistance = 1.7f;
		const float fDistanceScoreMultiplier = 200.0f;
		int i = -1;

		for(CGameRules::TPlayers::iterator it=players.begin();it!=players.end();++it)
		{
			i++;

			if(*it == spawnData.playerId)
				continue;

			const IEntity *pOther = gEnv->pEntitySystem->GetEntity(*it);
			Vec3  vDiffToEnemy			 = (pOther->GetWorldPos() - potentialSpawnPosition);
			vDiffToEnemy.z					+= max(2.0f * vDiffToEnemy.z, 6.0f);
			float fDistanceFromEnemy = vDiffToEnemy.len();
			
			float fDistanceScoreFraction = __fres(max(fDistanceFromEnemy - 4.f, 0.001f));

			float fScoreForThisEnemy = max(fScoreOnTopOfEnemy - (fDistanceFromEnemy * fMultiplierForEnemyDistance), 0.0f) + (fDistanceScoreFraction * fDistanceScoreMultiplier);

			SpawnLogAlways("[SPAWN] >>> Score from Enemy %d is %.6f at distance %.1f", i, fScoreForThisEnemy, fDistanceFromEnemy);

			fScoreFromEnemies = max(fScoreForThisEnemy, fScoreFromEnemies);
		}

		SpawnLogAlways("[SPAWN] >> Total Score from Enemies: %.6f", fScoreFromEnemies);
	}

	return fScoreFromEnemies;
}

float CGameRulesRSSpawning::GetScoreFromProximityToFriendlies(	SUsefulSpawnData& spawnData, TPositionList& FriendlyPlayerPositions,
																																TMultiplierList& FriendlyScoreMultipliers, const Vec3& potentialSpawnPosition )
{
	SpawnLogAlways("[SPAWN] > GetScoreFromProximityToFriendlies()");

	float	fScoreFromFriendlies = 0.0f;

	if(spawnData.numActiveFriendlyPlayers > 0)
	{
		const float fIdealMax = 30.0f;
		const float fIdealMin = 15.0f;

		const float fCloseFriendlyScoreMultiplier = 500.0f;
		const float fFarFriendlyScoreMultiplier		= 2.0f;

		float fClosestFriendlySq = FLT_MAX;
		int index = 0;

		for(int i = FriendlyPlayerPositions.size() - 1; i >= 0; i--)
		{
			Vec3 diff = (FriendlyPlayerPositions[i] - potentialSpawnPosition);
			
			//This will bias against using spawn positions near in X/Y but on a different level in Z
			float fZBiasDistanceFromFriendlySq = (diff.x * diff.x) + (diff.y * diff.y) + fabsf(diff.z * diff.z * diff.z);

			if(fZBiasDistanceFromFriendlySq < fClosestFriendlySq)
			{
				fClosestFriendlySq = fZBiasDistanceFromFriendlySq;
				index = i;
			}
		}

		float fClosestFriendly = sqrt_fast_tpl(fClosestFriendlySq);

		if(fClosestFriendly < fIdealMin)
		{	
			const float fAbortDist = 0.7f; //If the sapwn is within this distance of a friendly, we're going to be intersecting - abort
			const float fDiff = (fIdealMin - fClosestFriendly);
			const float fDistanceScore = (float)__fsel(fDiff - fAbortDist, fDiff, sqr(fDiff) * 10.0f);
			fScoreFromFriendlies =  fDiff * fCloseFriendlyScoreMultiplier * FriendlyScoreMultipliers[index];
		}
		else
		{
			const float fIdealSub = (fIdealMin + fIdealMax) * 0.5f;
			const float fIdealRadius = fabsf(fIdealMax - fIdealSub);
			fScoreFromFriendlies = max(fabsf(fClosestFriendly - fIdealSub) - fIdealRadius, 0.0f) * fFarFriendlyScoreMultiplier;
		}

		SpawnLogAlways("[SPAWN] >> Total Score from Friendlies: %.6f, closest was at %.2f, spawn time modifier %s", fScoreFromFriendlies, fClosestFriendly, FriendlyScoreMultipliers[index] < 1.0f ? "APPLIED" : "NOT APPLIED");
	}
	
	return fScoreFromFriendlies;
}

float CGameRulesRSSpawning::GetScoreFromProximityToExclusionZones( SUsefulSpawnData& spawnData, EntityId spawnId)
{
	SpawnLogAlways("[SPAWN] > GetScoreFromProximityToExclusionZones()");

	if(spawnData.isInitialSpawn || DoesSpawnLocationRespectPOIs(spawnId))
	{
		SpawnLogAlways("[SPAWN] >> Not in exclusion zone, no additional score");
		return 0.0f;
	}
	else
	{
		SpawnLogAlways("[SPAWN] >> IN EXCLUSION ZONE, score of %.6f added", fScoreForPOICollision);
		return fScoreForPOICollision;
	}
}

float CGameRulesRSSpawning::GetScoreFromProximityToIdealLocation( SUsefulSpawnData& spawnData, const Vec3& idealLocation, const Vec3& potentialSpawnPosition )
{
	SpawnLogAlways("[SPAWN] > GetScoreFromProximityToIdealLocation()");
	const float fLocationScoreDistanceMultiplier = 0.4f;
	float fScoreFromLocation = (idealLocation - potentialSpawnPosition).len() * fLocationScoreDistanceMultiplier;
	SpawnLogAlways("[SPAWN] >> Score from ideal location: %.6f", fScoreFromLocation);
	return fScoreFromLocation;
}

void CGameRulesRSSpawning::UpdateEnemyPlayerPositions( SUsefulSpawnData& spawnData, TPositionList& EnemyPlayerPositions )
{
	int				idx						= 0;
	EntityId	enemyPlayerId = 0;

	while( enemyPlayerId = m_pGameRules->GetTeamActivePlayer(spawnData.enemyTeamId, idx++))
	{
		const IEntity * pEnemyPlayer = gEnv->pEntitySystem->GetEntity(enemyPlayerId);

		EnemyPlayerPositions.push_back(pEnemyPlayer->GetPos());
	}
}

void CGameRulesRSSpawning::UpdateFriendlyPlayerPositionsAndMultipliers( SUsefulSpawnData& spawnData, TPositionList& FriendlyPlayerPositions, TMultiplierList& FriendlyPlayerMultipliers )
{
	int				idx								= 0;
	EntityId	friendlyPlayerId	= 0;

	const float fCurrentTime = gEnv->pTimer->GetFrameStartTime().GetMilliSeconds();
	const float fDurationWithNoProximityPenalty = 5.0f * 1000.0f;

	while( friendlyPlayerId = m_pGameRules->GetTeamActivePlayer(spawnData.playerTeamId, idx++))
	{
		const IEntity * pFriendlyPlayer = gEnv->pEntitySystem->GetEntity(friendlyPlayerId);

		float fPlayerScoreMultiplier = 1.0f;

		TPlayerDataMap::iterator it = m_playerValues.find(friendlyPlayerId);
		if (it != m_playerValues.end())
		{
			SPlayerData& friendlyPlayerData = it->second;

			float fTimeSinceRevived = fCurrentTime - friendlyPlayerData.lastRevivedTime;

			//If the player has spawned within fDurationWithNoProximityPenalty seconds, there is no
			//	score penalty for spawners near them, to encourage groups spawning together
			fPlayerScoreMultiplier = (float)__fsel(fTimeSinceRevived - fDurationWithNoProximityPenalty, 1.0f, 0.0f);
		}

		FriendlyPlayerMultipliers.push_back(fPlayerScoreMultiplier);

		FriendlyPlayerPositions.push_back(pFriendlyPlayer->GetPos());
	}
}

float CGameRulesRSSpawning::GetScoreFromLineOfSight( SUsefulSpawnData& spawnData, EntityId spawnId )
{
	SpawnLogAlways("[SPAWN] > GetScoreFromLineOfSight()");
	
	if(m_spawningVisTable.IsSpawnLocationVisibleByTeam(spawnId, spawnData.enemyTeamId))
	{
		SpawnLogAlways("[SPAWN] >> Spawner visible! Adding score of %.2f", fScoreForSpawnerBeingVisible);
		return fScoreForSpawnerBeingVisible;
	}
	else
	{
		return 0.0f;
	}
}

EntityId CGameRulesRSSpawning::GetSpawnLocationNonTeamGame(EntityId playerId, const Vec3 &deathPos)
{
#if MONITOR_BAD_SPAWNS
	m_DBG_spawnData.scoringResults.clear();
#endif

	EntityId bestSpawnerId = 0;

	const bool bCheckLineOfSight = !gEnv->IsDedicated();

	TSpawnLocations& spawnLocations = GetSpawnLocations(playerId);
	if (m_pGameRules->GetLivingPlayerCount() > 0)
	{
		float fBestScore = FLT_MAX;

		SUsefulSpawnData spawnData;

		PopulateSpawnData(spawnData, playerId);

		int iSpawnIndex = 0;
		
		for (TSpawnLocations::const_iterator it=spawnLocations.begin(); it!=spawnLocations.end(); ++it)
		{
			SpawnLogAlways("[SPAWN] Spawn #%d", iSpawnIndex);

			EntityId spawnId(*it);

			const IEntity *pSpawn = gEnv->pEntitySystem->GetEntity(spawnId);

			Vec3 spawnPosition = pSpawn->GetPos();

			const float fScoreFromEnemies					= GetScoreFromProximityToEnemiesNoTeams(spawnData, spawnPosition);
			const float fScoreFromExclusionZones	= GetScoreFromProximityToExclusionZones(spawnData, spawnId);
			const float fScoreFromLineOfSight			= bCheckLineOfSight ? GetScoreFromLineOfSight(spawnData, spawnId) : 0.f;

			const float fScore = fScoreFromEnemies + fScoreFromExclusionZones + fScoreFromLineOfSight;

#if MONITOR_BAD_SPAWNS
			m_DBG_spawnData.scoringResults.push_back(SScoringResults(fScoreFromEnemies, fScoreFromLineOfSight > 0, fScoreFromExclusionZones > 0));
#endif

			if(fScore < fBestScore)
			{
				if(IsSpawnLocationSafe(spawnData.playerId, pSpawn, 1.0f, false, 0.0f))
				{
					SpawnLogAlways("[SPAWN] >> Total score %.6f, better than previous best of %.6f", fScore, fBestScore);
					bestSpawnerId = spawnId;
					fBestScore		= fScore;
#if MONITOR_BAD_SPAWNS
					m_DBG_spawnData.iSelectedSpawnIndex = iSpawnIndex;
					m_DBG_spawnData.selectedSpawn				= spawnId;
#endif
				}
				else
				{
					SpawnLogAlways("[SPAWN] >> Total score %.6f, better than %.6f but UNSAFE", fScore, fBestScore);
				}		
			}
			else
			{
				SpawnLogAlways("[SPAWN] >> Total score %.6f, too high, not using", fScore);
			}

			iSpawnIndex++;
		}
	}
	else
	{
		bool isInitialSpawn = IsPlayerInitialSpawning(playerId);
		bestSpawnerId = GetRandomSpawnLocation_Cached(spawnLocations, playerId, 0, isInitialSpawn);
	}

	return bestSpawnerId;
}

void CGameRulesRSSpawning::UpdateSpawnPointAverage(const EntityId spawnId, float& fNumSpawns, Vec3& averagePos) const
{
	fNumSpawns += 1.0f;

	const IEntity *pSpawn = gEnv->pEntitySystem->GetEntity(spawnId);

	Vec3 spawnPosition = pSpawn->GetPos();

	float fInvNumSpawns = __fres(fNumSpawns);
	float fScaleRemainder = 1.0f - fInvNumSpawns;

	averagePos = (spawnPosition * fInvNumSpawns) + (averagePos * fScaleRemainder);
}
