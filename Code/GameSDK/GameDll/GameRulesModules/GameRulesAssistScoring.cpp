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

#include "StdAfx.h"
#include "GameRulesAssistScoring.h"
#include "GameRules.h"
#include "GameRulesTypes.h"
#include "IGameRulesScoringModule.h"
#include "IGameRulesPlayerStatsModule.h"
#include "Player.h"
#include "PersistantStats.h"
#include "Network/Lobby/GameLobby.h"

#if (0 && !defined(_RELEASE))
	#define DEBUG_ASSIST_SCORING 1
#else
	#define DEBUG_ASSIST_SCORING 0
#endif

#if DEBUG_ASSIST_SCORING
#define DbgLog CryLog
#else
#define DbgLog(...) (void)(0)
#endif

CGameRulesAssistScoring::CGameRulesAssistScoring()
{
	m_pGameRules = NULL;
	m_maxTimeBetweenAttackAndKillForAssist = 0.0f;
	m_maxAssistScore = 0;
	m_assistScoreMultiplier = 0.f;
	m_bTeamSpecificScoring = false;
}

CGameRulesAssistScoring::~CGameRulesAssistScoring()
{
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterKillListener(this);
	}
}

void CGameRulesAssistScoring::Init(XmlNodeRef xml)
{
	m_pGameRules = g_pGame->GetGameRules();

	if (!xml->getAttr("maxTimeBetweenAttackAndKillForAssist", m_maxTimeBetweenAttackAndKillForAssist))
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpawning failed to find valid maxTimeBetweenAttackAndKillForAssist param");
	}
	int iValue = 0;
	if (xml->getAttr("teamSpecificScoring", iValue))
	{
		m_bTeamSpecificScoring = (iValue != 0);
	}

	if (!xml->getAttr("maxAssistScore", m_maxAssistScore))
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpawning failed to find valid maxAssistScore param");
	}

	if (!xml->getAttr("assistScoreMultiplier", m_assistScoreMultiplier))
	{
		CRY_ASSERT_MESSAGE(0, "CGameRulesMPSpawning failed to find valid assistScoreMultiplier param");
	}

	CGameRules* pGameRules = g_pGame->GetGameRules();
	pGameRules->RegisterKillListener(this);
}

void CGameRulesAssistScoring::RegisterAssistTarget(EntityId targetId)
{
	DbgLog("CGameRulesAssistScoring::RegisterAssistTarget()");
	m_targetAttackerMap.insert(TTargetAttackerMap::value_type(targetId, TAttackers()));
}

void CGameRulesAssistScoring::UnregisterAssistTarget(EntityId targetId)
{
	DbgLog("CGameRulesAssistScoring::UnregisterAssistTarget()");
	stl::member_find_and_erase(m_targetAttackerMap, targetId);
}

void CGameRulesAssistScoring::OnEntityHit(const HitInfo &info, const float tagDuration)
{
	DbgLog("CGameRulesAssistScoring::OnEntityHit()");

	// Cheap early checks for being injured by self or injured anonymously... do this before looking up team
	if (info.shooterId && info.shooterId != info.targetId)
	{
		const CGameRules::eThreatRating threat = m_pGameRules->GetThreatRating(info.shooterId, info.targetId);
		// Only need to look up target's team if shooter is on a team
		if(threat==CGameRules::eHostile)
		{
			DbgLog("CGameRulesAssistScoring::OnEntityHit() - Entity %d '%s' on team %d injured enemy entity %d '%s'", info.shooterId, m_pGameRules->GetEntityName(info.shooterId), m_pGameRules->GetTeam(info.shooterId), info.targetId, m_pGameRules->GetEntityName(info.targetId));

			TTargetAttackerMap::iterator it=m_targetAttackerMap.find(info.targetId);
			if (it!=m_targetAttackerMap.end())
			{
				TAttackers::const_iterator end_it = it->second.end();
				TAttackers::iterator vit=it->second.begin();
				while(vit!=end_it)
				{
					SAttackerData &data = *vit;
					if (data.m_entityId == info.shooterId)
					{
						float serverTime = m_pGameRules->GetServerTime();

						//Delete too old entries
						SAttackerData::TDamageTimes::iterator beginDelete_it = data.m_damageTimes.begin();
						SAttackerData::TDamageTimes::iterator delete_it = beginDelete_it;
						SAttackerData::TDamageTimes::iterator lastDelete_it;
						SAttackerData::TDamageTimes::const_iterator endDelete_it = data.m_damageTimes.end();
						float fEarliestTime = serverTime - m_maxTimeBetweenAttackAndKillForAssist;
						bool mustDelete = false;

						while( (delete_it!=endDelete_it) && (delete_it->m_time < fEarliestTime))
						{
							mustDelete = true;
							++delete_it;
							lastDelete_it = delete_it;
						}

						if (mustDelete)
						{
							data.m_damageTimes.erase(beginDelete_it, lastDelete_it);
						}

						//Add new damage-time
						if (tagDuration<0.f)
						{
							data.m_lastShootTime = serverTime;
							data.m_damageTimes.push_back(SAttackerData::SDamageTime(info.damage, serverTime));
						}
						else
						{
							data.m_tagExpirationTime = serverTime + tagDuration*1000.f;
						}

						break;
					}
					++vit;
				}

				if (vit==end_it)
				{
					SAttackerData newAttacker(info.shooterId, m_pGameRules->GetServerTime(), tagDuration, info.damage);
					it->second.push_back(newAttacker);
					DbgLog("CGameRulesAssistScoring::OnEntityHit() has found a new attacker %s who's hit this entity %s. Saving time %f",  m_pGameRules->GetEntityName(info.shooterId), m_pGameRules->GetEntityName(info.targetId), newAttacker.m_time);
				}
			}
		}
	}
}

//void CGameRulesAssistScoring::ClAwardAssistKillPoints(EntityId victimId)
//{
//	IGameRulesScoringModule *scoringModule = m_pGameRules->GetScoringModule();
//	int playerPoints=scoringModule->GetPlayerPointsByName("assistKill");
//	DbgLog("CGameRulesAssistScoring::ClAwardAssistKillPoints()");
//	m_pGameRules->IncreasePoints(g_pGame->GetIGameFramework()->GetClientActorId(), SGameRulesScoreInfo(EGRST_PlayerKillAssist, playerPoints).AttachVictim(victimId));
//}

void CGameRulesAssistScoring::SvDoScoringForDeath(const EntityId victimId, const EntityId shooterId, const char *weaponClassName, int damage, int material, int hit_type)
{
	DbgLog("CGameRulesAssistScoring::SvDoScoringForDeath()");
	CRY_ASSERT_MESSAGE(gEnv->bServer, "CGameRulesAssistScoring::SvDoScoringForDeath() should only be called on the server");

	TTargetAttackerMap::iterator it=m_targetAttackerMap.find(victimId);
	if(it!=m_targetAttackerMap.end())
	{
		if (!m_pGameRules->HasGameActuallyStarted())
		{
			it->second.clear();
			return;
		}

		float time=m_pGameRules->GetServerTime();
		float fEarliestTimeForAssistShoot = time - m_maxTimeBetweenAttackAndKillForAssist;

		bool hadAssist=false;
		bool awardedSuicideKill=false;

		for (TAttackers::reverse_iterator vit=it->second.rbegin(); vit!=it->second.rend(); ++vit)
		{
			SAttackerData &data = *vit;
			CRY_ASSERT_MESSAGE(data.m_entityId, string().Format("List of %s's recent attackers includes entity ID 0! Why has a NULL entity been allocated a slot in the list?", m_pGameRules->GetEntityName(victimId)));
			if ( (data.m_lastShootTime > fEarliestTimeForAssistShoot) || (time < data.m_tagExpirationTime))
			{
				DbgLog("CGameRulesAssistScoring::SvDoScoringForDeath() found entity %d %s has attacked the dead entity %s recently enough to be considered for assist", data.m_entityId, m_pGameRules->GetEntityName(data.m_entityId), m_pGameRules->GetEntityName(victimId));
				if (shooterId == data.m_entityId)
				{
					DbgLog("CGameRulesAssistScoring::SvDoScoringForDeath() found the attacking player %s actually killed the entity %s, NOT considering them for assist bonus", m_pGameRules->GetEntityName(data.m_entityId), m_pGameRules->GetEntityName(victimId));
				}
				else
				{
					hadAssist=true;

					DbgLog("CGameRulesAssistScoring::SvDoScoringForDeath() has found player %s has assisted in killing entity %s, awarding assist points", m_pGameRules->GetEntityName(data.m_entityId), m_pGameRules->GetEntityName(victimId));
					IGameRulesScoringModule *scoringModule = m_pGameRules->GetScoringModule();
					CRY_ASSERT_MESSAGE(scoringModule, "CGameRulesAssistScoring::SvDoScoringForDeath() failed to find a scoring module, this should NOT happen");
					if (scoringModule)
					{
						EGRST type = EGRST_PlayerKillAssist;
						if (m_bTeamSpecificScoring)
						{
							int teamId = m_pGameRules->GetTeam(data.m_entityId);
							if (teamId == 1)
							{
								type = EGRST_PlayerKillAssistTeam1;
							}
							else
							{
								type = EGRST_PlayerKillAssistTeam2;
							}
						}
						
						TGameRulesScoreInt playerPoints = 0;
						if (type == EGRST_PlayerKillAssist)
						{
							float totalAssistScoringDamage = 0.f;
							SAttackerData::TDamageTimes::const_reverse_iterator rend_it = vit->m_damageTimes.rend();
							float fEarliestTime = time - m_maxTimeBetweenAttackAndKillForAssist;
							for (SAttackerData::TDamageTimes::const_reverse_iterator dt_rit = vit->m_damageTimes.rbegin();
									dt_rit != rend_it && (dt_rit->m_time >= fEarliestTime);
									++dt_rit)
							{
								totalAssistScoringDamage += dt_rit->m_damage;
							}

							playerPoints = (TGameRulesScoreInt)(totalAssistScoringDamage * m_assistScoreMultiplier);
							playerPoints = (playerPoints< m_maxAssistScore)? playerPoints: m_maxAssistScore;
						}
						else
						{
							playerPoints = scoringModule->GetPlayerPointsByType(type);
						}

						if(IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(victimId))
						{
							if(pActor->IsPlayer())
							{
								if( time <= data.m_tagExpirationTime )
								{
									if (playerPoints==0)
									{
										type = EGRST_Tagged_PlayerKillAssist;
									}
									playerPoints += scoringModule->GetPlayerPointsByType(EGRST_Tagged_PlayerKillAssist);
								}
							}
						}
						
						IGameRulesPlayerStatsModule *pPlayerStatsModule = m_pGameRules->GetPlayerStatsModule();
						if (pPlayerStatsModule)
						{
							pPlayerStatsModule->IncrementAssistKills(data.m_entityId);
						}

						m_pGameRules->IncreasePoints(data.m_entityId, SGameRulesScoreInfo(type, playerPoints).AttachVictim(victimId));
					}
				}
			}
		}

		if (!hadAssist)
		{
			IActor *pShooterActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(shooterId);
			if (pShooterActor != NULL && pShooterActor->IsPlayer() && m_pGameRules->GetThreatRating(shooterId, victimId)==CGameRules::eHostile)
			{
				CPersistantStats::GetInstance()->IncrementStatsForActor(shooterId, EIPS_KillsWithoutAssist, 1);
			}
		}

		it->second.clear();
	}
}

void CGameRulesAssistScoring::ClHandleAssistsForDeath(const EntityId victimId, const EntityId shooterId)
{
	DbgLog("CGameRulesAssistScoring::ClHandleAssistsForDeath()");
	TTargetAttackerMap::iterator it=m_targetAttackerMap.find(victimId);
	if (it!=m_targetAttackerMap.end())
	{
		float time=m_pGameRules->GetServerTime();

		bool hadAssist=false;
		bool awardedSuicideKill=false;

		for (TAttackers::reverse_iterator vit=it->second.rbegin(); vit!=it->second.rend(); ++vit)
		{
			SAttackerData &data = *vit;
			CRY_ASSERT_MESSAGE(data.m_entityId, string().Format("List of %s's recent attackers includes entity ID 0! Why has a NULL entity been allocated a slot in the list?", m_pGameRules->GetEntityName(victimId)));
			float fEarliestTime = time - m_maxTimeBetweenAttackAndKillForAssist;
			if(data.m_lastShootTime >= fEarliestTime)
			{
				DbgLog("CGameRulesAssistScoring::ClHandleAssistsForDeath() found entity %d %s has attacked the dead player %s recently enough to be considered for assist", data.m_entityId, m_pGameRules->GetEntityName(data.m_entityId), m_pGameRules->GetEntityName(victimId));
				if (shooterId == data.m_entityId)
				{
					DbgLog("CGameRulesAssistScoring::ClHandleAssistsForDeath() found the attacking player %s actually killed the player %s, NOT considering them for assist bonus", m_pGameRules->GetEntityName(data.m_entityId), m_pGameRules->GetEntityName(victimId));
				}
				else
				{
					hadAssist=true;
					break;
				}
			}
		}
		
		bool isShooterClient = (shooterId == gEnv->pGameFramework->GetClientActorId());
		if (!hadAssist && isShooterClient)
		{
			IActor *pShooterActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(shooterId);
			if (pShooterActor != NULL && pShooterActor->IsPlayer() && m_pGameRules->GetThreatRating(shooterId, victimId)==CGameRules::eHostile)
			{
				CPersistantStats::GetInstance()->IncrementStatsForActor(shooterId, EIPS_KillsWithoutAssist, 1);
			}
		}

		it->second.clear();
	}
}

void CGameRulesAssistScoring::OnEntityKilled(const HitInfo &hitInfo)
{
	DbgLog("CGameRulesAssistScoring::OnEntityKilled()");
	if (gEnv->bServer)
	{
		char weaponClassName[128];
		if (!g_pGame->GetIGameFramework()->GetNetworkSafeClassName(weaponClassName, sizeof(weaponClassName), hitInfo.weaponClassId))
		{
			cry_strcpy(weaponClassName, "unknown weapon");
		}

		EntityId victimId = hitInfo.targetId;

		IEntity* pVictimEntity = gEnv->pEntitySystem->GetEntity(victimId);
		CRY_ASSERT_MESSAGE(pVictimEntity, "failed to resolve our victim that has been killed!!!");

		if(pVictimEntity)
		{
			SvDoScoringForDeath(victimId, hitInfo.shooterId, weaponClassName, (int)hitInfo.damage, hitInfo.partId, hitInfo.type);
		}
	}
	else
	{
		EntityId victimId = hitInfo.targetId;

		IEntity *pVictimEntity = gEnv->pEntitySystem->GetEntity(victimId);
		CRY_ASSERT_MESSAGE(pVictimEntity, "failed to resolve our victim that has been killed!!!");

		if (pVictimEntity)
		{
			ClHandleAssistsForDeath(victimId, hitInfo.shooterId);
		}
	}
}

EntityId CGameRulesAssistScoring::SvGetMostRecentAttacker(EntityId targetId)
{
	DbgLog("CGameRulesAssistScoring::SvGetMostRecentAttacker");
	CRY_ASSERT_MESSAGE(gEnv->bServer, "CGameRulesAssistScoring::SvGetMostRecentAttacker should only be called on the server");

	TTargetAttackerMap::iterator it=m_targetAttackerMap.find(targetId);

	if (it==m_targetAttackerMap.end())
	{
		CRY_ASSERT_MESSAGE(0, string().Format("CGameRulesAssistScoring::SvGetMostRecentAttacker failed to find a targetAttacker mapping element for target %s", m_pGameRules->GetEntityName(targetId)));
	}
	else
	{
		float time=m_pGameRules->GetServerTime();

		if (!it->second.empty())
		{
			TAttackers::const_iterator vit = it->second.begin();
			TAttackers::const_iterator endit = it->second.end();
			TAttackers::const_iterator foundit = endit;
			
			float serverTime = m_pGameRules->GetServerTime();
			float fEarliestScoringTime = serverTime - m_maxTimeBetweenAttackAndKillForAssist;
			float fMostRecentAttackerTime = -1.f;

			while (vit!=endit) 
			{
				const SAttackerData &data = *vit;
				if ( (data.m_lastShootTime > fMostRecentAttackerTime)
					&& (data.m_lastShootTime >= fEarliestScoringTime ))
				{
					fMostRecentAttackerTime = data.m_lastShootTime;
					foundit = vit;
				}

				++vit;
			}

			if (foundit!= endit)
			{
				return foundit->m_entityId;
			}
		}
	}
	return 0;
}



#undef DEBUG_ASSIST_SCORING 
#undef DbgLog
