// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 04:09:2009  : Created by James Bamford

*************************************************************************/

#ifndef __IGAMERULESSPAWNINGMODULE_H__
#define __IGAMERULESSPAWNINGMODULE_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesSystem.h"

class IGameRulesSpawningModule
{
public:
	struct SPlayerData
	{
		SPlayerData() : 
			deathTime(-10000000.f), // negative time is given so first spawn definitely occurs. When autotesting server time was not ticking much in frontend and stopping spawning happening as it was not long enough since death!
			deathPos(ZERO), 
			lastRevivedTime(1000000.f), //highly positive time so that the first spawn does not trend away from friendly players 
			lastSpawnLocationId(0),
			lastKillerId(0),
			triedAutoRevive(false)
		{};

		float deathTime;
		Vec3 deathPos;
		float lastRevivedTime;
		EntityId lastSpawnLocationId;
		EntityId lastKillerId;
		bool triedAutoRevive;
	};

	typedef std::map<EntityId, SPlayerData>	TPlayerDataMap;

public:
	virtual ~IGameRulesSpawningModule() {}

	virtual void Init(XmlNodeRef xml) = 0;
	virtual void PostInit() = 0;
	virtual void Update(float frameTime) = 0;
	virtual void OnStartGame() {}
	virtual void SetLastSpawn(EntityId playerId, EntityId spawnId) = 0;

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) = 0;

	virtual void AddSpawnLocation(EntityId location, bool isInitialSpawn, bool doVisTest, const char *pGroupName) = 0;
	virtual void RemoveSpawnLocation(EntityId id, bool isInitialSpawn) = 0;
	virtual void EnableSpawnLocation(EntityId location, bool isInitialSpawn, const char *pGroupName) = 0;
	virtual void DisableSpawnLocation(EntityId id, bool isInitialSpawn) = 0;
	
	virtual EntityId GetSpawnLocation(EntityId playerId) = 0;
	virtual EntityId GetFirstSpawnLocation(int teamId) const = 0;
	virtual int GetSpawnLocationCount() const = 0;
	virtual EntityId GetNthSpawnLocation(int idx) const = 0;
	virtual int GetSpawnIndexForEntityId(EntityId spawnId) const =0;

	virtual void SetInitialSpawnGroup(const char* groupName) =0;

	virtual void AddAvoidPOI(EntityId entityId, float avoidDistance, bool enabled, bool bStaticPOI=false) = 0;
	virtual void RemovePOI(EntityId entityId) = 0;
	virtual void EnablePOI(EntityId entityId) = 0;
	virtual void DisablePOI(EntityId entityId) = 0;

	virtual void PlayerJoined(EntityId playerId) = 0;
	virtual void PlayerLeft(EntityId playerId) = 0;
	virtual void OnPlayerKilled(const HitInfo &hitInfo) = 0;

	virtual void ClRequestRevive(EntityId playerId) = 0;
	virtual bool SvRequestRevive(EntityId playerId, EntityId preferredSpawnId = 0) = 0;
	virtual void PerformRevive(EntityId playerId, int teamId, EntityId preferredSpawnId) = 0;
	virtual void OnSetTeam(EntityId playerId, int teamId) = 0;

	virtual const TPlayerDataMap* GetPlayerValuesMap(void) const = 0;

	virtual void ReviveAllPlayers(bool isReset, bool bOnlyIfDead) = 0;
	virtual void ReviveAllPlayersOnTeam(int teamId) {}

	virtual int  GetRemainingLives(EntityId playerId) = 0;
	virtual int  GetNumLives() = 0;

	virtual float GetTimeFromDeathTillAutoReviveForTeam(int teamId) const = 0;
	virtual float GetPlayerAutoReviveAdditionalTime(IActor* pActor) const = 0; 
	virtual float GetAutoReviveTimeScaleForTeam(int teamId) const = 0;
	virtual void SetAutoReviveTimeScaleForTeam(int teamId, float newScale) = 0;

	virtual void HostMigrationInsertIntoReviveQueue(EntityId playerId, float timeTillRevive) = 0;

	virtual void OnInGameBegin() = 0;

	virtual void OnInitialEquipmentScreenShown() = 0;
	virtual void OnNewRoundEquipmentScreenShown() = 0;
	virtual float GetRemainingInitialAutoSpawnTimer() = 0;

	virtual bool SvIsMidRoundJoiningAllowed() const = 0;
	virtual bool CanPlayerSpawnThisRound(const EntityId playerId) const = 0;

	virtual bool IsInInitialChannelsList(uint32 channelId) const = 0;

	virtual void HostMigrationStopAddingPlayers() = 0;
	virtual void HostMigrationResumeAddingPlayers() = 0;
};
#endif  // __IGAMERULESSPAWNINGMODULE_H__
