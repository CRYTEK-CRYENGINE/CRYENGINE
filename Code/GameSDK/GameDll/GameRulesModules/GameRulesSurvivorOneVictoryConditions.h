// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#define SURVIVOR_ONE_ENABLED 0

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Game rules module to handle victory upon being the last player left
	-------------------------------------------------------------------------
	History:
	- 06:11:2009  : Created by Thomas Houghton

*************************************************************************/
#if SURVIVOR_ONE_ENABLED

#ifndef _GAME_RULES_SURVIVOR_ONE_CONDITIONS_PLAYER_H_
#define _GAME_RULES_SURVIVOR_ONE_CONDITIONS_PLAYER_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameRulesStandardVictoryConditionsPlayer.h"
#include "GameRulesModules/IGameRulesRoundsListener.h"
#include <CryCore/Containers/CryFixedArray.h>

class CGameRules;

class CGameRulesSurvivorOneVictoryConditions :	
									public CGameRulesStandardVictoryConditionsPlayer,
									public IGameRulesRoundsListener
{
private:
	typedef CGameRulesStandardVictoryConditionsPlayer  inherited;

protected:
	int  m_svLatestSurvCount;
	EntityId  m_svLatestSurvList[MAX_PLAYER_LIMIT];

	typedef struct SRadarPing
	{
		float timeLimit;
		float pingTime; // pingTime > 0.f triggers pings with that frequency

		SRadarPing()
		{
			timeLimit=0.f;
			pingTime=0.f;
		}
	};

	typedef CryFixedArray<SRadarPing, 3> TRadarPingArray;
	TRadarPingArray m_radarPingArray;
	unsigned int m_radarPingStage;
	float m_radarPingStageTimer;
	float m_radarPingTimer;
	int m_scoreIncreasePerElimination;

public:

public:
	CGameRulesSurvivorOneVictoryConditions();
	~CGameRulesSurvivorOneVictoryConditions();

	virtual void Init(XmlNodeRef xml);
	virtual void Update(float frameTime);

	// IGameRulesVictoryConditionsModule
	virtual void	ClVictoryTeam(int teamId, EGameOverReason reason, ESVC_DrawResolution winningResolution, const SDrawResolutionData& level1, const SDrawResolutionData& level2, const EntityId killedEntity, const EntityId shooterEntity) {};
	virtual void	ClVictoryPlayer(int playerId, EGameOverReason reason, const EntityId killedEntity, const EntityId shooterEntity);
	// ~IGameRulesVictoryConditionsModule

	// IGameRulesSurvivorCountListener
	virtual void SvSurvivorCountRefresh(int count, const EntityId survivors[], int numKills);
	// ~IGameRulesSurvivorCountListener

	// IGameRulesRoundsListener
	virtual void OnRoundStart();
	virtual void OnRoundEnd();
	virtual void OnSuddenDeath();
	virtual void ClRoundsNetSerializeReadState(int newState, int curState) {}
	virtual void OnRoundAboutToStart() {}
	// ~IGameRulesRoundsListener

protected:
	void UpdateRadarPings(float frameTime);
	virtual void OnEndGamePlayer(EntityId playerId, EGameOverReason reason);

	// CGameRulesStandardVictoryConditionsPlayer
	virtual void TimeLimitExpired();
	// ~CGameRulesStandardVictoryConditionsPlayer

};

#endif // _GAME_RULES_SURVIVOR_ONE_CONDITIONS_PLAYER_H_

#endif // SURVIVOR_ONE_ENABLED