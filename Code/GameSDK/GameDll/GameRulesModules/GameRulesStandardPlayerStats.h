// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Game rules module to handle player scores and stats
	-------------------------------------------------------------------------
	History:
	- 03:09:2009  : Created by Ben Johnson

*************************************************************************/

#ifndef _GAME_RULES_PLAYER_STATS_H_
#define _GAME_RULES_PLAYER_STATS_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesPlayerStatsModule.h"
#include "IGameRulesRevivedListener.h"
#include "IGameRulesRoundsListener.h"

class CGameRules;

class CGameRulesStandardPlayerStats :	public IGameRulesPlayerStatsModule,
																			public IGameRulesRevivedListener,
																			public IGameRulesRoundsListener
{
public:
	CGameRulesStandardPlayerStats();
	virtual ~CGameRulesStandardPlayerStats();

	// IGameRulesPlayerStatsModule
	virtual void	Init(XmlNodeRef xml);
	virtual void	Reset(); // TODO : call
	virtual void	Update(float frameTime);

	virtual void	OnStartNewRound();

	virtual bool	NetSerialize( EntityId playerId, TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

	virtual void	CreatePlayerStats(EntityId playerId, uint32 channelId);
	virtual void	RemovePlayerStats(EntityId playerId);
	virtual void	ClearAllPlayerStats();

	virtual void	OnPlayerKilled(const HitInfo &info);
	virtual void	IncreasePoints(EntityId playerId, int amount);
	virtual void	IncreaseGamemodePoints(EntityId playerId, int amount);
	virtual void	ProcessSuccessfulExplosion(EntityId playerId, float damageDealt = 0.0f, uint16 projectileClassId = 0);
	virtual void	IncrementAssistKills(EntityId playerId);
#if ENABLE_PLAYER_KILL_RECORDING
	virtual void	IncreaseKillCount( const EntityId playerId, const EntityId victimId );
#endif // ENABLE_PLAYER_KILL_RECORDING
	
	virtual int		GetNumPlayerStats() const;
	virtual const SGameRulesPlayerStat*  GetNthPlayerStats(int n);
	virtual const SGameRulesPlayerStat* GetPlayerStats(EntityId playerId);

	virtual void SetEndOfGameStats(const CGameRules::SPlayerEndGameStatsParams &inStats);

	virtual void CalculateSkillRanking();

	virtual void RecordSurvivalTime(EntityId playerId, float timeSurvived);
	// ~IGameRulesPlayerStatsModule
	
	// IGameRulesRevivedListener
	virtual void EntityRevived(EntityId entityId);
	// ~IGameRulesRevivedListener

	// IGameRulesRoundsListener
	virtual void OnRoundStart() {}
	virtual void OnRoundEnd();
	virtual void OnSuddenDeath() {}
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {}
	virtual void OnRoundAboutToStart() {}
	// ~IGameRulesRoundsListener

protected:
	static const float GR_PLAYER_STATS_PING_UPDATE_INTERVAL;

	SGameRulesPlayerStat * GetPlayerStatsInternal(EntityId playerId);
	void SendHUDExplosionEvent();

	void IncrementWeaponHitsStat(const char *pWeaponName);

	//typedef std::map<EntityId, SGameRulesPlayerStat> TPlayerStats;
	typedef std::vector<SGameRulesPlayerStat> TPlayerStats;

	TPlayerStats m_playerStats;

	float m_lastUpdatedPings;
	int m_dbgWatchLvl;

  uint16 m_classidC4;
  uint16 m_classidFlash;
  uint16 m_classidFrag;
	uint16 m_classidLTAG;
	uint16 m_classidJawRocket;

	bool m_bRecordTimeSurvived;
};

#endif // _GAME_RULES_PLAYER_STATS_H_
