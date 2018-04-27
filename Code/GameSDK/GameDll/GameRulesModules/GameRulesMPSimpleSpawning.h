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

#ifndef __GAMERULES_MP_SIMPLE_SPAWNING_H__
#define __GAMERULES_MP_SIMPLE_SPAWNING_H__

#include "GameRulesMPSpawning.h"

//Define to control the logging of spawning code in the CGameRulesRSSpawning class
#define SPAWN_LOGGING 0

#if SPAWN_LOGGING
#define SpawnLogAlways CryLogAlways
#else
#define SpawnLogAlways(...) ((void)0)
#endif

class CGameRulesRSSpawning : public CGameRulesMPSpawningBase
{
private:
	typedef CGameRulesMPSpawningBase inherited;
	typedef std::vector<Vec3> TPositionList;
	typedef std::vector<float>	TMultiplierList;

	struct SUsefulSpawnData
	{
		EntityId	playerId;
		int				playerTeamId;
		int				numActiveFriendlyPlayers;

		int				enemyTeamId;
		int				numActiveEnemyPlayers;
		bool			isInitialSpawn;
	};	

	void PopulateSpawnData(SUsefulSpawnData& spawnData, EntityId playerId);

	void GetEnemyTeamCentre(SUsefulSpawnData& spawnData, Vec3 * pOutCentre);
	void GetFriendlyTeamCentre(SUsefulSpawnData& spawnData, Vec3 * pOutCentre);
	void GetIdealSpawnLocation(SUsefulSpawnData& spawnData, const Vec3 * pEnemyCentre, const Vec3 * pFriendlyCentre, Vec3 * pOutIdealSpawnLoc);
	EntityId GetClosestSpawnToLocation(SUsefulSpawnData& spawnData, const Vec3 * pLocation);
	EntityId GetBestSpawnUsingWeighting(SUsefulSpawnData& spawnData, const Vec3 * pBestLocation);

	void UpdateMapCentre();

	void UpdateEnemyPlayerPositions(SUsefulSpawnData& spawnData, TPositionList& EnemyPlayerPositions);
	
	void UpdateFriendlyPlayerPositionsAndMultipliers(	SUsefulSpawnData& spawnData, TPositionList& FriendlyPlayerPositions,
																										TMultiplierList& FriendlyPlayerMultipliers);	

	float	GetScoreFromProximityToLastPrecachedSpawn(const Vec3& lastSpawnPos, const Vec3& spawnPos) const;
	float	GetScoreFromProximityToPreviousSpawn(const Vec3& lastSpawnPos, const Vec3& spawnPos) const;
	float GetScoreFromProximityToEnemies(SUsefulSpawnData& spawnData, TPositionList& EnemyPlayerPositions, const Vec3& potentialSpawnPosition);
	float GetScoreFromProximityToEnemiesNoTeams(SUsefulSpawnData& spawnData, const Vec3& potentialSpawnPosition);
	float GetScoreFromProximityToFriendlies(SUsefulSpawnData& spawnData, TPositionList& FriendlyPlayerPositions,
																					TMultiplierList& FriendlyPlayerMultipliers,const Vec3& potentialSpawnPosition);
	float GetScoreFromProximityToExclusionZones(SUsefulSpawnData& spawnData, EntityId spawnId);
	float GetScoreFromProximityToIdealLocation(SUsefulSpawnData& spawnData, const Vec3& idealLocation, const Vec3& potentialSpawnPosition);
	float GetScoreFromLineOfSight(SUsefulSpawnData& spawnData, EntityId spawnId);

public:
	CGameRulesRSSpawning();
	virtual ~CGameRulesRSSpawning();

protected:
	virtual EntityId GetSpawnLocationTeamGame(EntityId playerId, const Vec3 &deathPos);
	virtual EntityId GetSpawnLocationNonTeamGame(EntityId playerId, const Vec3 &deathPos);

	void UpdateSpawnPointAverage(const EntityId spawnId, float& fNumSpawns, Vec3& averagePos) const;

private:
	Vec3 m_mapCentre;
	bool m_initialized;
};

#endif // __GAMERULES_MP_SIMPLE_SPAWNING_H__