// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Game rules module to handle scoring points values
	-------------------------------------------------------------------------
	History:
	- 03:09:2009  : Created by Ben Johnson

*************************************************************************/

#ifndef _GAME_RULES_STANDARD_SCORING_H_
#define _GAME_RULES_STANDARD_SCORING_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesScoringModule.h"

class CGameRules;

class CGameRulesStandardScoring : public IGameRulesScoringModule
{
public:
	CGameRulesStandardScoring();
	virtual ~CGameRulesStandardScoring();

	// IGameRulesScoringModule
	virtual void	Init(XmlNodeRef xml);

	virtual bool NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags );

	virtual TGameRulesScoreInt GetPlayerPointsByType(EGRST pointsType) const;
	virtual TGameRulesScoreInt GetPlayerXPByType(EGRST pointsType) const;
	virtual TGameRulesScoreInt GetTeamPointsByType(EGRST pointsType) const;
	virtual void	DoScoringForDeath(IActor *pActor, EntityId shooterId, int damage, int material, int hit_type);
	virtual void	OnPlayerScoringEvent(EntityId playerId, EGRST type);
	virtual void  OnPlayerScoringEventWithInfo(EntityId playerId, SGameRulesScoreInfo* scoreInfo);
	virtual void  OnPlayerScoringEventToAllTeamWithInfo(const int teamId, SGameRulesScoreInfo* scoreInfo);
	virtual void	OnTeamScoringEvent(int teamId, EGRST pointsType);
	virtual void	SvResetTeamScore(int teamId);
	virtual int		GetStartTeamScore() { return m_startTeamScore; }
	virtual int		GetMaxTeamScore() { return m_maxTeamScore; }
	virtual void	SetAttackingTeamLost()			{ m_bAttackingTeamWonAllRounds = false; }
	virtual bool	AttackingTeamWonAllRounds() { return m_bAttackingTeamWonAllRounds; }
	virtual TGameRulesScoreInt GetDeathScoringModifier() { return m_deathScoringModifier; }
	virtual void SvSetDeathScoringModifier(TGameRulesScoreInt inModifier); 
	// ~IGameRulesScoringModule

	

	static const char * s_gamerulesScoreType[];

protected:
	void	InitScoreData(XmlNodeRef xml, TGameRulesScoreInt *scoringData, TGameRulesScoreInt *xpData);
	TGameRulesScoreInt GetPointsByType(const TGameRulesScoreInt *scoringData, EGRST type) const;
	bool	ShouldScore(CGameRules* pGameRules) const;

	TGameRulesScoreInt m_playerScorePoints[EGRST_Num];
	TGameRulesScoreInt m_playerScoreXP[EGRST_Num];
	TGameRulesScoreInt m_teamScorePoints[EGRST_Num];

	int m_maxTeamScore;
	int m_startTeamScore;
	bool m_useScoreAsTime; // Calculate score using time.
	bool m_bAttackingTeamWonAllRounds;

	TGameRulesScoreInt m_deathScoringModifier;
};

#endif // _GAME_RULES_STANDARD_SCORING_H_
