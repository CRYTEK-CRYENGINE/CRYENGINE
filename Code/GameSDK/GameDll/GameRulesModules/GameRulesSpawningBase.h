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

#ifndef __GAMERULES_SPAWNING_BASE_H__
#define __GAMERULES_SPAWNING_BASE_H__

#include "GameRulesModules/IGameRulesSpawningModule.h"
#include "GameRulesTypes.h"

class CGameRules;


class CGameRulesSpawningBase : public IGameRulesSpawningModule
{
private:
	typedef IGameRulesSpawningModule inherited;

protected:
	typedef std::vector<EntityId>								TSpawnLocations;

	struct SSpawnGroup
	{
		TSpawnLocations m_locations;
		uint32 m_id;
		uint8 m_useCount;
	};
	typedef std::vector<SSpawnGroup>						TSpawnGroups;

	static const uint32 kInvalidInitialSpawnIndex = ~0;

	TSpawnLocations			m_allSpawnLocations;
	TSpawnLocations			m_spawnLocations;
	TSpawnGroups				m_initialSpawnLocations;
	TPlayerDataMap			m_playerValues;
	CGameRules*					m_pGameRules;
	uint32							m_activeInitialSpawnGroupIndex;
	bool								m_bTeamAlwaysUsesInitialSpawns[2];

public:
	CGameRulesSpawningBase();
	virtual ~CGameRulesSpawningBase();

	virtual void SetLastSpawn(EntityId playerId, EntityId spawnId);

	virtual void Init(XmlNodeRef xml);
	virtual void PostInit();
	virtual void Update(float frameTime);

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags);

	virtual void AddSpawnLocation(EntityId location, bool isInitialSpawn, bool doVisTest, const char *pGroupName);
	virtual void RemoveSpawnLocation(EntityId id, bool isInitialSpawn);
	virtual void EnableSpawnLocation(EntityId location, bool isInitialSpawn, const char *pGroupName);
	virtual void DisableSpawnLocation(EntityId id, bool isInitialSpawn);

	virtual void SetInitialSpawnGroup(const char* groupName);
	
	bool	HasInitialSpawns() const { return !m_initialSpawnLocations.empty() && m_activeInitialSpawnGroupIndex != kInvalidInitialSpawnIndex; }
	const TEntityIdVec& GetInitialSpawns() const { return m_initialSpawnLocations[m_activeInitialSpawnGroupIndex].m_locations; }

	
	virtual EntityId GetSpawnLocation(EntityId playerId);
	virtual EntityId GetFirstSpawnLocation(int teamId) const;
	virtual int GetSpawnLocationCount() const;
	virtual EntityId GetNthSpawnLocation(int idx) const;
	virtual int GetSpawnIndexForEntityId(EntityId spawnId) const;

	virtual void AddAvoidPOI(EntityId entityId, float avoidDistance, bool enabled, bool bStaticPOI);
	virtual void RemovePOI(EntityId entityId);
	virtual void EnablePOI(EntityId entityId);
	virtual void DisablePOI(EntityId entityId);

	virtual void PlayerJoined(EntityId playerId);
	virtual void PlayerLeft(EntityId playerId);
	virtual void OnPlayerKilled(const HitInfo &hitInfo);

	virtual void ClRequestRevive(EntityId playerId);
	virtual bool SvRequestRevive(EntityId playerId, EntityId preferredSpawnId = 0);
	virtual void PerformRevive(EntityId playerId, int teamId, EntityId preferredSpawnId);
	virtual void OnSetTeam(EntityId playerId, int teamId) {}

	virtual const TPlayerDataMap* GetPlayerValuesMap(void) const { return &m_playerValues; }

	virtual void ReviveAllPlayers(bool isReset, bool bOnlyIfDead);
	
	virtual int  GetRemainingLives(EntityId playerId) { return 0; }
	virtual int  GetNumLives()  { return 0; }

	virtual ILINE float GetTimeFromDeathTillAutoReviveForTeam(int teamId) const { return -1.f; }
	virtual float GetPlayerAutoReviveAdditionalTime(IActor* pActor) const { return 0.f; }; 
	virtual float GetAutoReviveTimeScaleForTeam(int teamId) const { return 1.f; }
	virtual void SetAutoReviveTimeScaleForTeam(int teamId, float newScale) { }

	virtual void HostMigrationInsertIntoReviveQueue(EntityId playerId, float timeTillRevive) {}

	virtual void OnInGameBegin() {}
	virtual void OnInitialEquipmentScreenShown() {}
	virtual void OnNewRoundEquipmentScreenShown() {}
	virtual float GetRemainingInitialAutoSpawnTimer() { return -1.f; }
	virtual bool SvIsMidRoundJoiningAllowed() const { return true; };
	virtual bool CanPlayerSpawnThisRound(const EntityId playerId) const;
	virtual bool IsInInitialChannelsList(uint32 channelId) const { return true; }

	virtual void HostMigrationStopAddingPlayers() {};
	virtual void HostMigrationResumeAddingPlayers() {};

protected:
	ILINE float GetTime() const { return gEnv->pTimer->GetFrameStartTime().GetMilliSeconds(); }

	bool IsPlayerInitialSpawning(const EntityId playerId) const;
	TSpawnLocations& GetSpawnLocations(const EntityId playerId);
	static TSpawnLocations* GetSpawnLocationsFromGroup(TSpawnGroups &groups, const char *pGroupName);

	void SelectNewInitialSpawnGroup();

	int m_spawnGroupOverride; 
};



#endif // __GAMERULES_SPAWNING_BASE_H__
