// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for the game rule module to handle player scores and stats
	-------------------------------------------------------------------------
	History:
	- 03:09:2009  : Created by Ben Johnson

*************************************************************************/

#ifndef _GAME_RULES_PLAYER_STATS_MODULE_H_
#define _GAME_RULES_PLAYER_STATS_MODULE_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include <IGameRulesSystem.h>
#include "GameRules.h"

#define ENABLE_PLAYER_KILL_RECORDING 0

struct SGameRulesPlayerStat
{
	enum EFlags
	{
		PLYSTATFL_NULL											= (0),
		PLYSTATFL_HASSPAWNEDTHISROUND				= (1 << 0),
		PLYSTATFL_DIEDINEXTRATIMETHISROUND	= (1 << 1),
		PLYSTATFL_HASHADROUNDRESTART				= (1 << 2),
		PLYSTATFL_CANTSPAWNTHISROUND				= (1 << 3),
		PLYSTATFL_HASSPAWNEDTHISGAME				= (1 << 4),
		PLYSTATFL_USEINITIALSPAWNS					= (1 << 5),
	};

	SGameRulesPlayerStat(EntityId _playerId)
	{
		playerId = _playerId;
		Clear();
	}

	void Clear()
	{
		points = kills = assists = deaths = deathsThisRound = headshots = gamemodePoints = teamKills = ping = successfulFlashbangs = successfulFrags = successfulC4 = successfulLTAG = successfulJAW = flags = 0;
		damageDealt = timeSurvived = 0.0f;
		skillPoints = 0;
		bUseTimeSurvived = false;
	}

	void NetSerialize(TSerialize ser)
	{
		ser.Value("points", points, 'i32');
		ser.Value("kills", kills, 'ui16');
		ser.Value("assists", assists, 'ui16');
		ser.Value("deaths", deaths, 'ui16');
		ser.Value("deathsThisRound", deathsThisRound, 'ui16');
		ser.Value("teamKills", teamKills, 'ui16');
		ser.Value("headshots", headshots, 'ui16');
		ser.Value("gamemodePoints", gamemodePoints, 'i16');
		ser.Value("ping", ping, 'ui16');
		ser.Value("frag", successfulFrags, 'ui8');
    ser.Value("flash", successfulFlashbangs, 'ui8');
    ser.Value("c4", successfulC4, 'ui8');
		ser.Value("ltag", successfulLTAG, 'ui8');
		ser.Value("jaw", successfulJAW, 'ui8');
		ser.Value("damage", damageDealt, 'xdmg');
		ser.Value("flags", flags, 'ui8');
#if !defined(_RELEASE) && !PROFILE_PERFORMANCE_NET_COMPATIBLE
		bool bBranch = bUseTimeSurvived;
		ser.Value("bBranch", bBranch, 'bool');
		if (bBranch != bUseTimeSurvived)
		{
			CryFatalError("Invalid branch inside netserialise, please tell Colin (bBranch=%s)", bBranch ? "true" : "false");
		}
#endif
		// This branch is ok even inside a netserialise because the value of bUseTimeSurvived never changes for a given game
		if (bUseTimeSurvived)
		{
			ser.Value("timeSurvived", timeSurvived, 'ssfl');
		}
	}

#if ENABLE_PLAYER_KILL_RECORDING
	void IncrementTimesPlayerKilled( const EntityId otherPlayerId )
	{
		TKilledByCounter::iterator res = killedByCounts.find( otherPlayerId );
		if( res == killedByCounts.end() )
		{
			killedByCounts.insert(TKilledByCounter::value_type(otherPlayerId, 1));
			return;
		}

		res->second ++;
	}

	uint8 GetTimesKilledPlayer( const EntityId otherPlayerId ) const
	{
		TKilledByCounter::const_iterator res = killedByCounts.find( otherPlayerId );
		if( res == killedByCounts.end() )
		{
			return 0;
		}

		return res->second;
	}

	// TKilledByCounter will be as big as the number of entities killed.
	//   The registration for a player is not cleaned when a player exits.
	//   This may be desired behaviour.
	CRY_TODO( 09, 03, 2010, "Replace TKilledByCounter (a std::map) with something faster.")
	typedef std::map<EntityId, uint8> TKilledByCounter;
	TKilledByCounter killedByCounts;
#endif // ENABLE_PLAYER_KILL_RECORDING

	EntityId playerId;
	int32 points;
	uint16 kills;
	uint16 assists;
	uint16 deaths;
	uint16 deathsThisRound;
	uint16 teamKills;
	uint16 headshots;
	int16 gamemodePoints;
	uint16 ping;
	uint16 skillPoints;			// Not serialized with the other stats, this one is only sent when the game finishes
	uint8 successfulFrags;
  uint8 successfulFlashbangs;
  uint8 successfulC4;
	uint8 successfulLTAG;
	uint8 successfulJAW;
	float damageDealt;
	float timeSurvived;			// Only used in hunter gamemode - time survived as a marine this round (0.f if started as a hunter)
	bool bUseTimeSurvived;
	uint8 flags;  // see EFlags above
};

class IGameRulesPlayerStatsModule
{
public:
	virtual ~IGameRulesPlayerStatsModule() {}

	virtual void	Init(XmlNodeRef xml) = 0;
	virtual void	Reset() = 0;
	virtual void	Update(float frameTime) = 0;

	virtual void	OnStartNewRound() = 0;

	virtual bool	NetSerialize( EntityId playerId, TSerialize ser, EEntityAspects aspect, uint8 profile, int flags ) = 0;

	virtual void	CreatePlayerStats(EntityId playerId, uint32 channelId) = 0;
	virtual void	RemovePlayerStats(EntityId playerId) = 0;

	virtual void	ClearAllPlayerStats() = 0;

	virtual void	OnPlayerKilled(const HitInfo &info) = 0;
	virtual void	IncreasePoints(EntityId playerId, int amount) = 0;
	virtual void	IncreaseGamemodePoints(EntityId playerId, int amount) = 0;
	virtual void	ProcessSuccessfulExplosion(EntityId playerId, float damageDealt = 0.0f, uint16 projectileClassId = 0) = 0;
	virtual void	IncrementAssistKills(EntityId playerId) = 0;
#if ENABLE_PLAYER_KILL_RECORDING
	virtual void	IncreaseKillCount( const EntityId playerId, const EntityId victimId ) = 0;
#endif // ENABLE_PLAYER_KILL_RECORDING

	virtual int		GetNumPlayerStats() const = 0;
	virtual const SGameRulesPlayerStat*  GetNthPlayerStats(int n) = 0;
	virtual const SGameRulesPlayerStat*  GetPlayerStats(EntityId playerId) = 0;

	virtual void SetEndOfGameStats(const CGameRules::SPlayerEndGameStatsParams &inStats) = 0;

	virtual void CalculateSkillRanking() = 0;
	virtual void RecordSurvivalTime(EntityId playerId, float timeSurvived) = 0;
};

#endif // _GAME_RULES_PLAYER_STATS_MODULE_H_
