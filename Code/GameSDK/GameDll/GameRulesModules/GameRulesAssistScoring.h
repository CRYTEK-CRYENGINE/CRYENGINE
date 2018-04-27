// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
Game rules module to handle scoring points values
-------------------------------------------------------------------------
History:
- 14:09:2009  : Created by James Bamford

*************************************************************************/

#ifndef __GAMERULESASSISTSCORING_H__
#define __GAMERULESASSISTSCORING_H__

#include "IGameRulesAssistScoringModule.h"
#include "IGameRulesKillListener.h"
#include "GameRulesTypes.h"

class CGameRules;

class CGameRulesAssistScoring : 
	public IGameRulesAssistScoringModule, 
	public IGameRulesKillListener
{
protected:
	CGameRules *m_pGameRules;
	float m_maxTimeBetweenAttackAndKillForAssist;
	float m_assistScoreMultiplier;
	TGameRulesScoreInt m_maxAssistScore;

	struct SAttackerData
	{	
		struct SDamageTime
		{
			float m_damage;
			float m_time;

			SDamageTime(float damage, float time)
			{
				m_damage = damage;
				m_time = time;
			}
		};
		typedef std::vector<SDamageTime>			TDamageTimes;

		TDamageTimes m_damageTimes;
		EntityId m_entityId;
		float m_lastShootTime;
		float m_tagExpirationTime;

		SAttackerData(EntityId entityId, float time, float tagDuration, float damageDone )
		{
			m_entityId = entityId;

			if (tagDuration<0.f)
			{
				m_tagExpirationTime=-1.f;
				m_lastShootTime = time;
				m_damageTimes.push_back(SDamageTime(damageDone, time));
			}
			else
			{
				m_lastShootTime = -1.f;
				m_tagExpirationTime = time + tagDuration*1000.f;
			}
		}

	};
	
	typedef std::vector<SAttackerData>					TAttackers;
	typedef std::map<EntityId, TAttackers>			TTargetAttackerMap;

	TTargetAttackerMap m_targetAttackerMap;

public:
	CGameRulesAssistScoring();
	virtual ~CGameRulesAssistScoring();

	virtual void	Init(XmlNodeRef xml);

	virtual void	RegisterAssistTarget(EntityId targetId);
	virtual void	UnregisterAssistTarget(EntityId targetId);
	virtual void  OnEntityHit(const HitInfo &info, const float tagDuration=-1.f );
	virtual EntityId	SvGetMostRecentAttacker(EntityId targetId);

	//IGameRulesKillListener
	virtual void OnEntityKilledEarly(const HitInfo& hitInfo) {};
	virtual void OnEntityKilled(const HitInfo &hitInfo);
	//~IGameRulesKillListener

protected:
	void SvDoScoringForDeath(const EntityId victimId, const EntityId shooterId, const char *weaponClassName, int damage, int material, int hit_type);
	void ClHandleAssistsForDeath(const EntityId victimId, const EntityId shooterId);

	bool m_bTeamSpecificScoring;
};

#endif // __GAMERULESASSISTSCORING_H__