// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
History:
- 25:05:2010		Created by Jim Bamford
*************************************************************************/

#include "StdAfx.h"
#include "AfterMatchAwards.h"

#include "Utility/CryDebugLog.h"
#include "PersistantStats.h"
#include <CryGame/IGameFramework.h>
#include "Actor.h"
#include "GameRules.h"
#include "GameRulesModules/IGameRulesPlayerStatsModule.h"
#include "UI/UICVars.h"
#include "UI/UIManager.h"
#include "Network/Lobby/GameLobby.h"

#include "GameRulesModules/GameRulesObjective_PowerStruggle.h"

static AUTOENUM_BUILDNAMEARRAY(s_afterMatchAwardsNames, AfterMatchAwards);

#define GET_AWARD_FLAGS(a,b,c) b,
#define GET_AWARD_FLOAT_DATA(a,b,c) c,

const static uint32 s_awardFlags[EAMA_Max] = 
{
	AfterMatchAwards(GET_AWARD_FLAGS)
};

const static float s_awardFloatData[EAMA_Max] = 
{
	AfterMatchAwards(GET_AWARD_FLOAT_DATA)
};

#undef GET_AWARD_FLAGS
#undef GET_AWARD_FLOAT_DATA

#define DbgLog(...) CRY_DEBUG_LOG(AFTER_MATCH_AWARDS, __VA_ARGS__)
#define DbgLogAlways(...) CRY_DEBUG_LOG_ALWAYS(AFTER_MATCH_AWARDS, __VA_ARGS__)

const float CAfterMatchAwards::k_timeOutWaitingForClients = 3.0f;

CAfterMatchAwards::SAwardsForPlayer::SAwardsForPlayer(EntityId inPlayerId)
{
	DbgLog("CAfterMatchAwards::SAwardsForPlayer::SAwardsForPlayer()");

	Clear();

	m_playerEntityId = inPlayerId;

}

void CAfterMatchAwards::SAwardsForPlayer::Clear()
{
	DbgLog("CAfterMatchAwards::SAwardsForPlayer::Clear()");

	m_awards.clear();
	memset(m_working, 0, sizeof(m_working));

	m_playerEntityId=0;
	m_awardsReceivedFromClient=false;
}

CAfterMatchAwards::CAfterMatchAwards()
{
	DbgLog("CAfterMatchAwards::CAfterMatchAwards()");
	
	//m_haveAwards=false;
	m_persistantStats=NULL;

	m_playerClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("Player");

	Clear();
}

CAfterMatchAwards::~CAfterMatchAwards()
{
	DbgLog("CAfterMatchAwards::~CAfterMatchAwards()");
}

void CAfterMatchAwards::Clear()
{
	DbgLog("CAfterMatchAwards::Clear()");

	m_actorAwards.clear();
	m_state = k_state_waiting_for_game_to_end;
	m_localClientEntityIdWas = 0;
	m_timeOutLeftWaitingForClients = k_timeOutWaitingForClients;
}

void CAfterMatchAwards::Init()
{
	DbgLog("CAfterMatchAwards::Init()");

	m_persistantStats = CPersistantStats::GetInstance();
}

void CAfterMatchAwards::Update(const float dt)
{
	if (gEnv->bServer)
	{
		switch(m_state)
		{
			case k_state_server_game_ended_waiting_for_all_client_results:
			{
				// TODO - add timeout here incase we never receive all the clients info?

				int numReceived=0;	
				int numPresent=0;

				bool hasLocalClient = gEnv->IsClient();

				m_timeOutLeftWaitingForClients -= dt;

				ActorAwardsMap::const_iterator it = m_actorAwards.begin();
				ActorAwardsMap::const_iterator end = m_actorAwards.end();
				for ( ; it!=end; ++it)
				{
					IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(it->first);
					if (pActor)
					{
						numPresent++;

						if (it->second.m_awardsReceivedFromClient)
						{
							numReceived++;
						}
					}
				}

				if(hasLocalClient)
				{
					numReceived++; // local client doesn't need receiving
				}

				DbgLog("CAfterMatchAwards::Update() state k_state_server_game_ended_waiting_for_all_client_results numReceived=%d; size=%d; timeOutLeft=%f", numReceived, numPresent, m_timeOutLeftWaitingForClients);
				if (m_timeOutLeftWaitingForClients <= 0.0f)
				{
					DbgLog("CAfterMatchAwards::Update() timeOut has occurred waiting for clients to send results. haveReceived=%d; waitingForNum=%d. Continuing anyway", numReceived, numPresent);
				}

				CRY_ASSERT_MESSAGE(m_timeOutLeftWaitingForClients > 0.f, string().Format("AfterMatchAwards Update() - timeOut has occurred waiting for awards from clients. haveReceived=%d; waitingForNum=%d. Continuing anyways with incomplete results", numReceived, numPresent));

				if (numReceived == (numPresent) || m_timeOutLeftWaitingForClients <= 0.f) 
				{
					DbgLog("CAfterMatchAwards::Update() in state k_state_server_game_ended_waiting_for_all_client_results - and has received all the stats from all the clients - calculating stats");
					CalculateAwards();
					SvSendAwardsToClients();

					if(hasLocalClient)
					{
						HaveGotAwards();
					}
					else
					{
						m_state = k_state_game_ended_have_awards;
					}
				}
				break;
			}
		}
	}
	else
	{
		switch (m_state)
		{
			case k_state_client_game_ended_waiting_for_awards:
				break;
		}

	}
}

CAfterMatchAwards::SAwardsForPlayer *CAfterMatchAwards::GetAwardsForActor(EntityId actorId)
{
	// needs to be called now after the game has finished so an invalid actor has to be allowed
	IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(actorId);
	if(!pActor || !pActor->IsPlayer())
		return NULL;

	CRY_ASSERT_MESSAGE(actorId, "GetAwardsForActor() has been passed an invalid NULL entityID.");
	if (actorId == 0)
	{
		DbgLog("CAfterMatchAwards::GetAwardsForActor() passed in actorId=0 returning rather than poluting map!!");
		return NULL;
	}

	ActorAwardsMap::iterator it = m_actorAwards.find(actorId);
	if(it != m_actorAwards.end())
	{
		return &it->second;
	}
	else
	{
		SAwardsForPlayer awards(actorId);
		ActorAwardsMap::iterator inserted = m_actorAwards.insert(ActorAwardsMap::value_type(actorId, awards)).first;
		return &inserted->second;
	}
}

void CAfterMatchAwards::CalculateAwardForActor(EAfterMatchAwards inAward, EntityId inActorId)
{
	DbgLog("CAfterMatchAwards::CalculateAwardForActor() inAward=%d (%s) inActorId=%d (%s)", inAward, GetNameForAward(inAward), inActorId, GetEntityName(inActorId));
	SAwardsForPlayer *awards=GetAwardsForActor(inActorId);
	CRY_ASSERT_MESSAGE(awards, "CalculateAwardForActor() failed to get award for actor!!");
	if (awards)
	{
		SAwardsForPlayer::SWorkingEle &result = awards->m_working[inAward];
		switch(inAward)
		{
			case EAMA_MostValuable:
				result.m_data.m_float = m_persistantStats->GetDerivedStatForActorThisSession(EDFPS_KillDeathRatio, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Ninja:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_SuitStealthTime, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_SpeedDemon:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_DistanceSprint, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_IsItABird:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_DistanceAir, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_MostMotivated:
			{
				CGameRules *pGameRules = g_pGame->GetGameRules();
				IGameRulesPlayerStatsModule *playerStats = pGameRules ? pGameRules->GetPlayerStatsModule() : NULL;
				CRY_ASSERT_MESSAGE(playerStats, "CalculateAwardForActor() MostMotivated award requires the playerStats");
				if (playerStats)
				{
					const SGameRulesPlayerStat* statsForPlayer=playerStats->GetPlayerStats(inActorId);
					CRY_ASSERT_MESSAGE(statsForPlayer, string().Format("CalculateAwardForActor() MostMotivated award, failed to find stats for actor=%s", GetEntityName(inActorId)));
					if (statsForPlayer)
					{
						result.m_data.m_float = (float)statsForPlayer->points;
						result.m_calculated=true;
					}
				}
				break;
			}
			case EAMA_Untouchable:
				// TODO - this really should be stored in an int!!! float for now for network send
				result.m_data.m_float = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_Deaths, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Untouchable2:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_DamageTaken, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Adaptable:
				result.m_data.m_float = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_SuitModeChanges, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_ArmourPlating:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_ArmourHits, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_ClayPigeon:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_InAirDeaths, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Murderiser:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EMPS_KillsByDamageType, "melee", inActorId);
				result.m_calculated=true;
				break;
      case EAMA_MoneyShot:
        result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_WinningKill, inActorId);
        result.m_calculated=true;
        break;
			case EAMA_WrongPlaceWrongTime:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_VictimOnFinalKillcam, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_TargetLocked:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_TaggedEntities, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_EnergeticBunny:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_EnergyUsed, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Rampage:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(ESIPS_Kills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_AidingRadar:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_TeamRadar, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Exhibitionist:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_SkillKills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Bing:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_HeadShotKills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_BangOn:
			{
				int numShotsFired=m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_Shots, inActorId);
				// enforce 10 bullets fired for this award
				if (numShotsFired > 10)
				{
					result.m_data.m_float = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDFPS_Accuracy, inActorId);
				}
				else
				{
					result.m_data.m_float = 0.f;
				}
				result.m_calculated=true;
				break;
			}
			case EAMA_BulletsCostMoney:
				result.m_data.m_float = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_Shots, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Warbird:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_WarBirdKills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_DemolitionMan:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EMPS_WeaponKills, "C4", inActorId);
				result.m_calculated=true;
				break;
			case EAMA_ToolsOfTheTrade:
				{
					int over3kills = 0;
					std::vector<int> stats;
					m_persistantStats->GetMapStatForActorThisSession(EMPS_WeaponKills,inActorId,stats);
					int size = stats.size();
					for ( int idx = 0; idx < size; idx++ )
					{
						if ( stats[idx] >= 3 )
						{
							over3kills++;
						}
					}
					result.m_data.m_float = (float)over3kills;
					result.m_calculated=true;
				}
				break;
			case EAMA_ProTips:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_CloakedReloads, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_BattlefieldSurgery:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_HealthRestore, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_SprayAndPray:
				result.m_data.m_float = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_Shots, inActorId);
				result.m_calculated=true;
				break;			
			case EAMA_Robbed:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_KillAssists, inActorId);
				result.m_calculated=true;
				break;			
			case EAMA_SingleMinded:
				result.m_data.m_float = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_Kills, inActorId);
				result.m_calculated=true;
				break;			
			case EAMA_NeverSayDie:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(ESIPS_Deaths, inActorId);
				result.m_calculated=true;
				break;			
			case EAMA_Spanked:
				result.m_data.m_float = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_Kills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Invincible:
				result.m_data.m_float = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_Deaths, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_DirtyDozen:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(ESIPS_Kills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_BulletMagnet:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_DamageTaken, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Punisher:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_DamageDelt, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Pacifist:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_DamageDelt, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Codpiece:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_Groinhits, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_DontPanic:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_FriendlyFires, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Pardon:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_GrenadeSurvivals, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_GlassJaw:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_MeleeDeaths, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_KeepYourHeadDown:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_TimeCrouched, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_StandardBearer:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_FlagCarriedTime, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_DugIn:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_CrashSiteHeldTime, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_LoneWolf:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_LoneWolfKills, inActorId);
				result.m_calculated=true;				
				break;
			case EAMA_Assassin:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_StealthKills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Impregnable:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_KillsSuitArmor, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_SafetyInNumbers:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_SafetyInNumbersKills, inActorId);
				result.m_calculated=true;				
				break;
			case EAMA_AdvancedRecon:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_VisorActiveTime, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_MostSelfish:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_KillsWithoutAssist, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Boing:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EMPS_KillsByDamageType, "stamp", inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Technomancer:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_IntelCollectedTime, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Observer:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_KillCamTime, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_XRayVision:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_BulletPenetrationKills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Mostcowardly:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_ShotsInMyBack, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_MostSneaky:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_ShotInBack, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_Relentless:
				{
					int highestKillsOfSinglePlayer = 0;
					std::vector<int> stats;
					m_persistantStats->GetMapStatForActorThisSession(EMPS_EnemyKilled,inActorId,stats);
					int size = stats.size();
					for ( int idx = 0; idx < size; idx++ )
					{
						if ( stats[idx] > highestKillsOfSinglePlayer )
						{
							highestKillsOfSinglePlayer = stats[idx];
						}
					}
					result.m_data.m_float = (float)highestKillsOfSinglePlayer;
					result.m_calculated=true;
				}
				break;
			case EAMA_Genocide:
				{
					std::vector<int> stats;
					m_persistantStats->GetMapStatForActorThisSession(EMPS_EnemyKilled,inActorId,stats);
					int size = stats.size();
					int enemiesKilled = 0;
					for ( int idx = 0; idx < size; idx++ )
					{
						if ( stats[idx] >= 1 )
						{
							enemiesKilled += 1;
						}
					}
					int enemyTeamCount = 0;
					CGameRules *pGameRules = g_pGame->GetGameRules();
					if ( pGameRules )
					{
						int clientTeam = pGameRules->GetTeam(inActorId);
						int opponentsTeam = 3 - clientTeam;
						enemyTeamCount = pGameRules->GetTeamPlayerCount(opponentsTeam);
					}
					result.m_data.m_float = (enemiesKilled >= enemyTeamCount && enemyTeamCount > 0) ? 1.0f : 0.0f;
					result.m_calculated=true;
				}
				break;
			case EAMA_Icy:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_DistanceSlid, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_BigBanger:
				{
					int highestKillsOfSingleExplosion = 0;
					std::vector<int> stats;
					m_persistantStats->GetMapStatForActorThisSession(EMPS_KillsFromExplosion,inActorId,stats);
					int size = stats.size();
					for ( int idx = 0; idx < size; idx++ )
					{
						if ( stats[idx] > highestKillsOfSingleExplosion )
						{
							highestKillsOfSingleExplosion = stats[idx];
						}
					}
					result.m_data.m_float = (float)highestKillsOfSingleExplosion;
					result.m_calculated=true;
				}
				break;
			case EAMA_MountieScore:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_MountedKills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_RipOff:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EIPS_UnmountedKills, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_LeapOfFaith:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession(EFPS_FallDistance, inActorId);
				result.m_calculated=true;
				break;
			case EAMA_WallOfSteel:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession( EFPS_WallOfSteel, inActorId );
				result.m_calculated = true;
				break;			
				
			case EAMA_PoleDancer:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession(EMPS_WeaponKills, POLE_WEAPON_NAME, inActorId );
				result.m_calculated = true;
				break;

			case EAMA_HighFlyer:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession( EIPS_TimeInVTOLs, inActorId );
				result.m_calculated = true;
				break;
		
			case EAMA_DeathFromAbove:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession( EIPS_MountedVTOLKills, inActorId );
				result.m_calculated = true;
				break;

			case EAMA_TripleA:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession( EFPS_DamageToFlyers, inActorId );
				result.m_calculated = true;
				break;

			case EAMA_Dolphin:
				result.m_data.m_float = m_persistantStats->GetStatForActorThisSession( EFPS_DistanceSwum, inActorId );
				result.m_calculated = true;
				break;

			case EAMA_HideAndSeek:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession( EIPS_HunterHideAndSeek, inActorId );
				result.m_calculated = true;
				break;

			case EAMA_TheHunter:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession( EIPS_TheHunter, inActorId );
				result.m_calculated = true;
				break;

			case EAMA_SoloSpear:
				if( CGameRulesObjective_PowerStruggle* pSpearsRules = static_cast<CGameRulesObjective_PowerStruggle*>( g_pGame->GetGameRules()->GetObjectivesModule() ) )
				{
					result.m_data.m_float = pSpearsRules->HasSoloCapturedSpear( inActorId ) ? 1.0f : 0.0f;
					result.m_calculated = true;
				}
				break;
				
			case EAMA_LowFlyingObject:
				result.m_data.m_float = (float)m_persistantStats->GetStatForActorThisSession( EIPS_ThrownWeaponKills, inActorId );
				result.m_calculated = true;
				break;


			default:
				DbgLog("CAfterMatchAwards::CalculateAwardForActor() unhandled award inAward=%d (%s)", inAward, GetNameForAward(inAward));
				break;
		}
		
		DbgLog("CAfterMatchAwards::CalculateAwardForActor() inAward=%d (%s) inActorId=%d (%s) float data=%f", inAward, GetNameForAward(inAward), inActorId, GetEntityName(inActorId), result.m_data.m_float);
	}
}

// TODO - this will return one person when two draw - we need to detect draws like this
EntityId CAfterMatchAwards::GetMaximumFloatFromWorking(EAfterMatchAwards inAward, float *outMaxFloat)
{
	EntityId winner=0;
	float maxFloat=-999999999.f;

	DbgLog("CAfterMatchAwards::GetMaximumFloatFromWorking() inAward=%s", GetNameForAward(inAward));

	ActorAwardsMap::const_iterator it = m_actorAwards.begin();
	ActorAwardsMap::const_iterator end = m_actorAwards.end();
	for ( ; it!=end; ++it)
	{
		if (!it->second.m_working[inAward].m_calculated)
		{
			DbgLog("CAfterMatchAwards::GetMaximumFloatFromWorking() has found award=%s hasn't been calculated for player=%s. This should NOT happen. Skipping", GetNameForAward(inAward), GetEntityName(it->first));
			continue;
		}

		float val = it->second.m_working[inAward].m_data.m_float;

		DbgLog("CAfterMatchAwards::GetMaximumFloatFromWorking() actor=%s; value=%f; maxFloat=%f", GetEntityName(it->first), val, maxFloat);
		if ( val > maxFloat || it == m_actorAwards.begin())
		{
			maxFloat = val;
			winner=it->first;
			DbgLog("CAfterMatchAwards::GetMaximumFloatFromWorking() actor=%s; value=%f; maxFloat=%f - NEW MAXIMUM saving", GetEntityName(it->first), val, maxFloat);
		}
	}

	DbgLog("CAfterMatchAwards::GetMaximumFloatFromWorking() returning winner actor=%s; maxFloat=%f", GetEntityName(winner), maxFloat);

	if (outMaxFloat)
	{
		*outMaxFloat = maxFloat;
	}

	return winner;
}

EntityId CAfterMatchAwards::GetMinimumFloatFromWorking(EAfterMatchAwards inAward)
{
	EntityId winner=0;
	float minFloat=999999999.f;
	const EntityId localActorId = gEnv->pGameFramework->GetClientActorId();
	const bool bIsLocalAward = GetFlagsForAward(inAward) & eAF_LocalClients;
	
	DbgLog("CAfterMatchAwards::GetMinimumFloatFromWorking() inAward=%s", GetNameForAward(inAward));

	ActorAwardsMap::const_iterator it = m_actorAwards.begin();
	ActorAwardsMap::const_iterator end = m_actorAwards.end();
	for ( ; it!=end; ++it)
	{
		if (bIsLocalAward && localActorId != it->first)
		{
			continue;
		}

		CRY_ASSERT_MESSAGE(it->second.m_working[inAward].m_calculated, string().Format("GetMinimumFloatFromWorking() has encountered an working value for player=%s award=%s that hasn't been calculated. This shouldn't happen!", GetEntityName(it->first), GetNameForAward(inAward)));
		if (!it->second.m_working[inAward].m_calculated)
		{
			DbgLog("CAfterMatchAwards::GetMinimumFloatFromWorking() has found award=%s hasn't been calculated for player=%s. This should NOT happen. Skipping", GetNameForAward(inAward), GetEntityName(it->first));
			continue;
		}
		float val = it->second.m_working[inAward].m_data.m_float;

		DbgLog("CAfterMatchAwards::GetMinimumFloatFromWorking() actor=%s; value=%f; minFloat=%f", GetEntityName(it->first), val, minFloat);
		if ( val < minFloat || it == m_actorAwards.begin())
		{
			minFloat = val;
			winner=it->first;
			DbgLog("CAfterMatchAwards::GetMinimumFloatFromWorking() actor=%s; value=%f; minFloat=%f - NEW MINIMUM saving", GetEntityName(it->first), val, minFloat);
		}
	}

	DbgLog("CAfterMatchAwards::GetMinimumFloatFromWorking() returning winner actor=%s; minFloat=%f", GetEntityName(winner), minFloat);
	return winner;
}

// add all actors to results who have a results value equal to val
// being used for floats whilst ints aren't networkable but makes more sense for ints
void CAfterMatchAwards::GetAllFromWorkingWithFloat(EAfterMatchAwards inAward, float inVal, ActorsVector &results)
{
	const EntityId localActorId = gEnv->pGameFramework->GetClientActorId();
	const bool bIsLocalAward = GetFlagsForAward(inAward) & eAF_LocalClients;
	
	DbgLog("CAfterMatchAwards::GetAllFromWorkingWithFloat() inAward=%s", GetNameForAward(inAward));

	ActorAwardsMap::const_iterator it = m_actorAwards.begin();
	ActorAwardsMap::const_iterator end = m_actorAwards.end();
	for ( ; it!=end; ++it)
	{
		if (bIsLocalAward && localActorId != it->first)
		{
			continue;
		}

		CRY_ASSERT_MESSAGE(it->second.m_working[inAward].m_calculated, string().Format("GetAllFromWorkingWithFloat() has encountered an working value for player=%s award=%s that hasn't been calculated. This shouldn't happen!", GetEntityName(it->first), GetNameForAward(inAward)));
		if (!it->second.m_working[inAward].m_calculated)
		{
			DbgLog("CAfterMatchAwards::GetAllFromWorkingWithFloat() has found award=%s hasn't been calculated for player=%s. This should NOT happen. Skipping", GetNameForAward(inAward), GetEntityName(it->first));
			continue;
		}

		float val = it->second.m_working[inAward].m_data.m_float;

		DbgLog("CAfterMatchAwards::GetAllFromWorkingWithFloat() actor=%s; value=%f; inVal=%f", GetEntityName(it->first), val, inVal);

		if (val == inVal)
		{
			DbgLog("CAfterMatchAwards::GetAllFromWorkingWithFloat() has found actor=%s value=%f and inVal=%f matching adding to results", GetEntityName(it->first), val, inVal);
			results.push_back(it->first);
		}
	}
}

void CAfterMatchAwards::GetAllFromWorkingGreaterThanFloat(EAfterMatchAwards inAward, float inVal, ActorsVector &results)
{
	const EntityId localActorId = gEnv->pGameFramework->GetClientActorId();
	const bool bIsLocalAward = GetFlagsForAward(inAward) & eAF_LocalClients;
	
	DbgLog("CAfterMatchAwards::GetAllFromWorkingGreaterThanFloat() inAward=%s", GetNameForAward(inAward));
	
	ActorAwardsMap::const_iterator it = m_actorAwards.begin();
	ActorAwardsMap::const_iterator end = m_actorAwards.end();
	for ( ; it!=end; ++it)
	{
		if (bIsLocalAward && localActorId != it->first)
		{
			continue;
		}

		CRY_ASSERT_MESSAGE(it->second.m_working[inAward].m_calculated, string().Format("GetAllFromWorkingGreaterThanFloat() has encountered an working value for player=%s award=%s that hasn't been calculated. This shouldn't happen!", GetEntityName(it->first), GetNameForAward(inAward)));
		if (!it->second.m_working[inAward].m_calculated)
		{
			DbgLog("CAfterMatchAwards::GetAllFromWorkingGreaterThanFloat() has found award=%s hasn't been calculated for player=%s. This should NOT happen. Skipping", GetNameForAward(inAward), GetEntityName(it->first));
			continue;
		}
		float val = it->second.m_working[inAward].m_data.m_float;

		DbgLog("CAfterMatchAwards::GetAllFromWorkingGreaterThanFloat() actor=%s; value=%f; inVal=%f", GetEntityName(it->first), val, inVal);

		if (val > inVal)
		{
			DbgLog("CAfterMatchAwards::GetAllFromWorkingGreaterThanFloat() actor=%s; has found value=%f is greater than inVal=%f", GetEntityName(it->first), val, inVal);
			results.push_back(it->first);
		}
	}
}

// Calculates the award working value for the local client actor only
void CAfterMatchAwards::ClCalculateAward(EAfterMatchAwards inAward)
{
	DbgLog("CAfterMatchAwards::ClCalculateAward() inAward=%s", GetNameForAward(inAward));

	CRY_ASSERT_MESSAGE(gEnv->IsClient(), "have to have a local client to try and calculate awards on clients");
	CRY_ASSERT_MESSAGE(GetFlagsForAward(inAward) & eAF_LocalClients, "award has to be an award calculated by local clients");

	if (GetFlagsForAward(inAward) & eAF_LocalClients)
	{
		DbgLog("CAfterMatchAwards::ClCalculateAward() inAward=%s - is an award to be calculated by local clients", GetNameForAward(inAward));

		EntityId localClientId = gEnv->pGameFramework->GetClientActorId();

		CalculateAwardForActor(inAward, localClientId);
	}
}

// returns number of actors who are awarded award.
int CAfterMatchAwards::SvCalculateAward(EAfterMatchAwards inAward)
{
	DbgLog("CAfterMatchAwards::SvCalculateAward() inAward=%d (%s)", inAward, GetNameForAward(inAward));

	ActorsVector results;
	results.clear();

	switch(inAward)
	{
		case EAMA_MostValuable:
		case EAMA_Ninja:
		case EAMA_SpeedDemon:
		case EAMA_IsItABird:
		case EAMA_Adaptable:
		case EAMA_ArmourPlating:
		case EAMA_ClayPigeon:
		case EAMA_Murderiser:
    case EAMA_MoneyShot:
		case EAMA_WrongPlaceWrongTime:
		case EAMA_TargetLocked:
		case EAMA_Rampage:
		case EAMA_AidingRadar:
		case EAMA_Exhibitionist:
		case EAMA_Magpie:
		case EAMA_Bing:
		case EAMA_BangOn:
		case EAMA_EnergeticBunny:
		case EAMA_Warbird:
		case EAMA_DemolitionMan:
		case EAMA_ToolsOfTheTrade:
		case EAMA_ProTips:
		case EAMA_BattlefieldSurgery:
		case EAMA_SprayAndPray:
		case EAMA_Robbed:
		case EAMA_SingleMinded:
		case EAMA_NeverSayDie:
		case EAMA_BulletMagnet:
		case EAMA_Punisher:
		case EAMA_Codpiece:
		case EAMA_DontPanic:
		case EAMA_Pardon:
		case EAMA_GlassJaw:
		case EAMA_KeepYourHeadDown:
		case EAMA_Assassin:
		case EAMA_LoneWolf:
		case EAMA_SafetyInNumbers:
		case EAMA_XRayVision:
		case EAMA_Mostcowardly:
		case EAMA_MostSneaky:
		case EAMA_Relentless:
		case EAMA_Genocide:
		case EAMA_Icy:
		case EAMA_BigBanger:
		case EAMA_MountieScore:
		case EAMA_RipOff:
		case EAMA_AdvancedRecon:
		case EAMA_MostSelfish:
		case EAMA_Boing:
		case EAMA_Technomancer:
		case EAMA_LeapOfFaith:
		case EAMA_MostMotivated:
		case EAMA_StandardBearer:
		case EAMA_DugIn:
		case EAMA_Impregnable:
		case EAMA_Observer:								
		case EAMA_WallOfSteel:
		case EAMA_PoleDancer:	
		case EAMA_HighFlyer:					
		case EAMA_DeathFromAbove:			
		case EAMA_TripleA:
		case EAMA_Dolphin:				
		case EAMA_LowFlyingObject:		
		{
			float maxFloat;
			EntityId winningEntityId = GetMaximumFloatFromWorking(inAward, &maxFloat);
			bool isValidWin=true;

			if (GetFlagsForAward(inAward) & eAF_StatHasFloatMinLimit)
			{
				float statMinLimit=GetFloatDataForAward(inAward);

				DbgLog("CAfterMatchAwards::SvCalculateAward() award=%s stat has a float min limit of %f; winning max stat float=%f", GetNameForAward(inAward), statMinLimit, maxFloat);

				if (maxFloat >= statMinLimit)
				{
					DbgLog("CAfterMatchAwards::SvCalculateAward() award=%s; winning stat float=%f is >= than statMinLimit=%f - this is a valid win", GetNameForAward(inAward), maxFloat, statMinLimit);
				}
				else
				{
					DbgLog("CAfterMatchAwards::SvCalculateAward() award=%s; winning stat float=%f is < than statMinLimit=%f - this is NOT a valid win", GetNameForAward(inAward), maxFloat, statMinLimit);
					isValidWin=false;
				}
			}

			if (isValidWin)
			{
				results.push_back(winningEntityId);
			}
			break;
		}
		case EAMA_Untouchable:
		case EAMA_Untouchable2:
		case EAMA_BulletsCostMoney:
		case EAMA_Pacifist:
			results.push_back(GetMinimumFloatFromWorking(inAward));
			break;
		case EAMA_Spanked:
		case EAMA_Invincible:
			GetAllFromWorkingWithFloat(inAward, 0.f, results);
			break;
		case EAMA_DirtyDozen:
			GetAllFromWorkingGreaterThanFloat(inAward, 11.1f, results);
			break;
		case EAMA_HideAndSeek:	
		case EAMA_TheHunter:
		case EAMA_SoloSpear:	
			GetAllFromWorkingGreaterThanFloat(inAward, 0.0f, results);
			break;
		default:
			DbgLog("CAfterMatchAwards::SvCalculateAward() called for unhandled award=%d (%s)", inAward, GetNameForAward(inAward));
			break;
	}

	int numResults = results.size();
	for (int i=0; i<numResults; i++)
	{
		EntityId actorId = results[i];

		SAwardsForPlayer *awards=GetAwardsForActor(actorId);
		if (awards)
		{
			//award->m_awards[inAward]++;

			bool duplicate=false;

			int awardsLen=awards->m_awards.size();
			for (int j=0; j<awardsLen; j++)
			{
				CRY_ASSERT_MESSAGE(awards->m_awards[j] != inAward, string().Format("CAfterMatchAwards::SvCalculateAward() found we are pushing back a duplicate award=%d (%s) this shouldn't happen", inAward, GetNameForAward(inAward)));
				if (awards->m_awards[j] == inAward)
				{
					duplicate=true;
				}
			}

			if (!duplicate)
			{
				awards->m_awards.push_back(inAward);
				DbgLog("CAfterMatchAwards::SvCalculateAward() adding award achievement for award=%d (%s) to actor=%d (%s)",
					inAward, GetNameForAward(inAward), actorId, GetEntityName(actorId));
			}
			else
			{
				DbgLog("CAfterMatchAwards::SvCalculateAward() NOT adding duplicate award achievement for award=%d (%s) to actor=%d (%s)",
					inAward, GetNameForAward(inAward), actorId, GetEntityName(actorId));
			}

		}
	}

	return numResults;
}

int CAfterMatchAwards::CalculateAward(EAfterMatchAwards inAward)
{
	DbgLog("CAfterMatchAwards::CalculateAward() inAward=%s; bServer=%d; bClient=%d", GetNameForAward(inAward), gEnv->bServer, gEnv->IsClient());
	int ret=0;

	if (gEnv->bServer)
	{
		if (GetFlagsForAward(inAward) & eAF_Server)
		{
			DbgLog("CAfterMatchAwards::CalculateAward() SERVER - inAward=%s is an award that needs calculating fully on the server", GetNameForAward(inAward));

			// Gather all the award workings for every actor
			IActorSystem *pActorSystem=gEnv->pGameFramework->GetIActorSystem();
			IActorIteratorPtr pActorIterator=pActorSystem->CreateActorIterator();
			while (CActor* pActor = (CActor*)pActorIterator->Next())
			{
				if(pActor->GetEntity()->GetClass() == m_playerClass)
				{
					CalculateAwardForActor(inAward, pActor->GetEntityId());
				}
			}

			// Actually calculate the award winner(s)
			ret=SvCalculateAward(inAward);		
		}
		else if (GetFlagsForAward(inAward) & eAF_LocalClients)
		{
			DbgLog("CAfterMatchAwards::CalculateAward() SERVER - inAward=%s is an award that needs calculating by every client", GetNameForAward(inAward));

			// Calculate award for the local player
			if (gEnv->IsClient())
			{
				ClCalculateAward(inAward);
			}

			// Actually calculate the award winner(s)
			ret=SvCalculateAward(inAward);		
		}
		else
		{
			DbgLog("CAfterMatchAwards::CalculateAward() inAward=%s; unhandled flags for award..", GetNameForAward(inAward));
		}
	}
	else if (gEnv->IsClient())
	{
		if (GetFlagsForAward(inAward) & eAF_LocalClients)
		{
			DbgLog("CAfterMatchAwards::CalculateAward() CLIENT - inAward=%s is an award that needs calculating by every client", GetNameForAward(inAward));
		
			ClCalculateAward(inAward);
		}
		else
		{
			DbgLog("CAfterMatchAwards::CalculateAward() CLIENT - inAward=%s is an award that needs calculating on the server and we're a client. Ignoring", GetNameForAward(inAward));
		}
	}
	else
	{
		DbgLog("CAfterMatchAwards::CalculateAward() unhandled state.. we're not a server or a client!!!");
		CRY_ASSERT_MESSAGE(0, "CalculateAward() unhandled state.. we're not a server or a client!!!");
	}

	return ret;
}

void CAfterMatchAwards::CalculateAwards()
{
	DbgLog("CAfterMatchAwards::CalculateAwards() bServer=%d", gEnv->bServer);

	CGameRules *pGameRules = g_pGame->GetGameRules();

	CalculateAward(EAMA_MostValuable);
	CalculateAward(EAMA_MostMotivated);
	CalculateAward(EAMA_Ninja);
	CalculateAward(EAMA_SpeedDemon);
	CalculateAward(EAMA_IsItABird);
	CalculateAward(EAMA_Untouchable);
	CalculateAward(EAMA_Untouchable2);
	CalculateAward(EAMA_Adaptable);
	CalculateAward(EAMA_ArmourPlating);
	CalculateAward(EAMA_ClayPigeon);
	CalculateAward(EAMA_Murderiser);
	CalculateAward(EAMA_MoneyShot);
	CalculateAward(EAMA_TargetLocked);
	CalculateAward(EAMA_Rampage);
	CalculateAward(EAMA_AidingRadar);
	CalculateAward(EAMA_Exhibitionist);
	CalculateAward(EAMA_Magpie);
	CalculateAward(EAMA_Bing);
	CalculateAward(EAMA_BangOn);
	CalculateAward(EAMA_BulletsCostMoney);
	CalculateAward(EAMA_EnergeticBunny);
	CalculateAward(EAMA_Warbird);
	CalculateAward(EAMA_DemolitionMan);
	CalculateAward(EAMA_ToolsOfTheTrade);
	CalculateAward(EAMA_ProTips);
	CalculateAward(EAMA_BattlefieldSurgery);
	CalculateAward(EAMA_SprayAndPray);
	CalculateAward(EAMA_Robbed);
	CalculateAward(EAMA_SingleMinded);
	CalculateAward(EAMA_NeverSayDie);

	CalculateAward(EAMA_Spanked);
	CalculateAward(EAMA_Invincible);
	CalculateAward(EAMA_DirtyDozen);
	CalculateAward(EAMA_BulletMagnet);
	CalculateAward(EAMA_Pacifist);
	CalculateAward(EAMA_Punisher);
	CalculateAward(EAMA_Codpiece);
	CalculateAward(EAMA_Pardon);
	CalculateAward(EAMA_GlassJaw);
	CalculateAward(EAMA_KeepYourHeadDown);
	CalculateAward(EAMA_SafetyInNumbers);
	CalculateAward(EAMA_XRayVision);
	CalculateAward(EAMA_Mostcowardly);
	CalculateAward(EAMA_MostSneaky);
	CalculateAward(EAMA_Relentless);
	CalculateAward(EAMA_Genocide);
	CalculateAward(EAMA_WrongPlaceWrongTime);
 	CalculateAward(EAMA_Icy);
	CalculateAward(EAMA_BigBanger);
	CalculateAward(EAMA_MountieScore);
	CalculateAward(EAMA_RipOff);
	CalculateAward(EAMA_LeapOfFaith);

	CalculateAward(EAMA_AdvancedRecon); 
	CalculateAward(EAMA_Boing); 
	CalculateAward(EAMA_Assassin);
	CalculateAward(EAMA_Impregnable);
	CalculateAward(EAMA_Observer);
	
	CalculateAward(EAMA_WallOfSteel);
	CalculateAward(EAMA_PoleDancer);
	CalculateAward(EAMA_HighFlyer);
	CalculateAward(EAMA_DeathFromAbove);
	CalculateAward(EAMA_TripleA);
	CalculateAward(EAMA_Dolphin);
	CalculateAward(EAMA_LowFlyingObject);

	if (pGameRules->GetTeamCount() > 1)
	{
		CalculateAward(EAMA_MostSelfish); 
		CalculateAward(EAMA_DontPanic);
		CalculateAward(EAMA_LoneWolf);
	}

	EGameMode mode = pGameRules->GetGameMode();

	switch(mode)
	{
		case eGM_CaptureTheFlag:
			DbgLog("CAfterMatchAwards::CalculateAwards() playing a game of capture the flag");
			CalculateAward(EAMA_StandardBearer);
			break;
		case eGM_CrashSite:
			DbgLog("CAfterMatchAwards::CalculateAwards() playing a game of crashsite");
			CalculateAward(EAMA_DugIn);
			break;
		case eGM_Assault:
			DbgLog("CAfterMatchAwards::CalculateAwards() playing a game of Assault");
			CalculateAward(EAMA_Technomancer);
			break;
		case eGM_PowerStruggle:
			DbgLog("CAfterMatchAwards::CalculateAwards() playing a game of Spears");
			CalculateAward(EAMA_SoloSpear);
			break;
		case eGM_Gladiator:
			DbgLog("CAfterMatchAwards::CalculateAwards() playing a game of Hunter");
			CalculateAward(EAMA_HideAndSeek);
			CalculateAward(EAMA_TheHunter);
			break;
	}

	// cache the local client so we can look at its awards later
	m_localClientEntityIdWas=gEnv->pGameFramework->GetClientActorId();
}

void CAfterMatchAwards::EnteredGame()
{
	DbgLog("CAfterMatchAwards::EnteredGame()");

	Clear();
}

void CAfterMatchAwards::StartCalculatingAwards()
{
	DbgLog("CAfterMatchAwards::StartCalculatingAwards() bServer=%d; bClient=%d", gEnv->bServer, gEnv->IsClient());

	if (gEnv->bServer)
	{
		DbgLog("CAfterMatchAwards::CalculateAwards() Server - changing state to wait for clients to send all results");
		m_state = k_state_server_game_ended_waiting_for_all_client_results;
		m_timeOutLeftWaitingForClients = k_timeOutWaitingForClients;

		// ensure that we have all the actors in our map
		IActorSystem *pActorSystem=gEnv->pGameFramework->GetIActorSystem();
		IActorIteratorPtr pActorIterator=pActorSystem->CreateActorIterator();
		while (CActor* pActor = (CActor*)pActorIterator->Next())
		{
			if(pActor->GetEntity()->GetClass() == m_playerClass)
			{
				GetAwardsForActor(pActor->GetEntityId());
			}
		}
	}
	else
	{
		DbgLog("CAfterMatchAwards::CalculateAwards() Client - calculating local awards and waiting for server to send back results");
		CalculateAwards();
		ClSendAwardsToServer();
		m_state = k_state_client_game_ended_waiting_for_awards;
	}
}

void CAfterMatchAwards::ClSendAwardsToServer()
{
	DbgLog("CAfterMatchAwards::ClSendAwardsToServer() - sending calculated awards for local client to server");

	EntityId localClientId = gEnv->pGameFramework->GetClientActorId();
	SAwardsForPlayer *awards = GetAwardsForActor(localClientId);

	CRY_ASSERT_MESSAGE(awards, "ClSendAwardsToServer() has failed to find the awards for local client");
	if (awards)
	{
		CGameRules::SAfterMatchAwardWorkingsParams rmiParams;
		rmiParams.m_numAwards=0;
		rmiParams.m_playerEntityId=localClientId;
		memset(rmiParams.m_awards, 0, sizeof(rmiParams.m_awards));

		for (int i=0; i<EAMA_Max; i++)
		{
			if (awards->m_working[i].m_calculated)
			{
				DbgLog("CAfterMatchAwards::ClSendAwardsToServer() - has found an award (%s) that has been calculated - sending its value=%f to server.", GetNameForAward((EAfterMatchAwards)i), awards->m_working[i].m_data.m_float);
		
				if (rmiParams.m_numAwards < rmiParams.k_maxNumAwards)
				{
					rmiParams.m_awards[rmiParams.m_numAwards].m_award = i;
					// TODO add ints when required
					rmiParams.m_awards[rmiParams.m_numAwards].m_workingValue = awards->m_working[i].m_data.m_float;
					rmiParams.m_numAwards++;
				}
			}
		}

		DbgLog("CAfterMatchAwards::ClSendAwardsToServer() sending RMI");
		g_pGame->GetGameRules()->GetGameObject()->InvokeRMI(CGameRules::SvAfterMatchAwardsWorking(), rmiParams, eRMI_ToServer);
	}
}

void CAfterMatchAwards::SvSendAwardsToClients()
{
	DbgLog("CAfterMatchAwards::SendAwardsToClients()");

	EntityId localClientId = gEnv->pGameFramework->GetClientActorId();
	ActorAwardsMap::const_iterator it = m_actorAwards.begin();
	ActorAwardsMap::const_iterator end = m_actorAwards.end();
	for ( ; it!=end; ++it)
	{
		bool isLocalClient = (it->first == localClientId);
		if (isLocalClient)
			continue;

		int len=it->second.m_awards.size();

		CGameRules::SAfterMatchAwardsParams rmiParams;
		rmiParams.m_numAwards=0;
		memset(rmiParams.m_awards, 0, sizeof(rmiParams.m_awards));
		
		if (len > rmiParams.k_maxNumAwards)
		{
			DbgLog("CAfterMatchAwards::SendAwardsToClients() has found more awards for client %s than max=%d; clamping to max", GetEntityName(it->first), rmiParams.k_maxNumAwards);
			len = rmiParams.k_maxNumAwards;
		}

		// randomly pick awards to send to client? - no only the client knows what awards he already has

		for (int i=0; i<len; i++)
		{
			DbgLog("sending awards to clients - Actor=%d; (%s) - Award=%s", it->first, GetEntityName(it->first), GetNameForAward(it->second.m_awards[i]));

			rmiParams.m_awards[rmiParams.m_numAwards] = it->second.m_awards[i];
			rmiParams.m_numAwards++;
		}

		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(it->first);
		if (pActor)
		{
			g_pGame->GetGameRules()->GetGameObject()->InvokeRMI(CGameRules::ClAfterMatchAwards(), rmiParams, eRMI_ToClientChannel, pActor->GetChannelId());
		}
		else
		{
			DbgLog("CAfterMatchAwards::SvSendAwardsToClients() has failed to resolve an actor, assuming client has disconnected. Can't send awards when we don't know their channelId");
		}
	}
}

int CAfterMatchAwards::GetNumClientAwards()
{
	int numAwards = 0;
	if (SAwardsForPlayer *pAwards = GetAwardsForActor(m_localClientEntityIdWas))
	{
		numAwards = (int)pAwards->m_awards.size();
	}

	return numAwards;
}

EAfterMatchAwards CAfterMatchAwards::GetNthClientAward(const int Nth)
{
	if (SAwardsForPlayer *pAwards = GetAwardsForActor(m_localClientEntityIdWas))
	{
		if (Nth >= 0 && Nth < (int)pAwards->m_awards.size())
		{
			return pAwards->m_awards[Nth];
		}
	}
	return EAMA_Invalid;
}

void CAfterMatchAwards::ClientReceivedAwards(int numAwards, const uint8 awardsArray[])
{
	SAwardsForPlayer *awards = GetAwardsForActor(m_localClientEntityIdWas);

#if ! defined _RELEASE
	CryLog("CAfterMatchAwards::ClientReceivedAwards() numAwards=%d; localactor=%s; awards=%p", numAwards, GetEntityName(m_localClientEntityIdWas), awards);
#endif

	CRY_ASSERT_MESSAGE(awards, "ClientReceivedAwards() requires us to find our awards for what was our local client!!");
	if (awards)
	{
		for (int i=0; i<numAwards; i++)
		{
			if (awardsArray[i] < EAMA_Max)
			{
				EAfterMatchAwards award = (EAfterMatchAwards)awardsArray[i];
#if ! defined _RELEASE
				CryLog("Award - %d %s", awardsArray[i], GetNameForAward(award));
#endif
				awards->m_awards.push_back(award);
			}
			else
			{
				DbgLog("CAfterMatchAwards::ClientReceivedAwards() has received a bad out of range award=%d", awardsArray[i]);
				CRY_ASSERT_MESSAGE(0, string().Format("CAfterMatchAwards::ClientReceivedAwards() has received a bad out of range award=%d", awardsArray[i]));
			}
		}
	}

	CRY_ASSERT(!gEnv->bServer);
	// Gameover message comes before the winning kill message on clients
	// clients have to wait to ensure they've received the kill message for the winning kill before testing if we made it
	m_persistantStats->HandleLocalWinningKills();
	
	HaveGotAwards();
}

void CAfterMatchAwards::ServerReceivedAwardsWorkingFromPlayer(EntityId playerEntityId, int numWorkingValues, const CGameRules::SAfterMatchAwardWorkingsParams::SWorkingValue workingValues[])
{
	DbgLog("CAfterMatchAwards::ServerReceivedAwardsWorkingFromPlayer() player=%d (%s); numWorkingValues=%d", playerEntityId, GetEntityName(playerEntityId), numWorkingValues);

	SAwardsForPlayer *awards = GetAwardsForActor(playerEntityId);
	CRY_ASSERT_MESSAGE(awards, string().Format("ServerReceivedAwardsWorkingFromPlayer() failed to find awards for player=%d (%s)", playerEntityId, GetEntityName(playerEntityId)));

	if (awards)
	{
		DbgLog("CAfterMatchAwards::ServerReceivedAwardsWorkingFromPlayer() found awards for player=%s", GetEntityName(playerEntityId));

		for (int i=0; i<numWorkingValues; i++)
		{
			if (workingValues[i].m_award < EAMA_Max)
			{
				DbgLog("%dth working value: award=%s; value=%f", i, GetNameForAward((EAfterMatchAwards)workingValues[i].m_award), workingValues[i].m_workingValue);
				// TODO - add support for ints when required
				awards->m_working[workingValues[i].m_award].m_data.m_float=workingValues[i].m_workingValue;
				awards->m_working[workingValues[i].m_award].m_calculated=true;		// not really necessary
			}
			else
			{
				CRY_ASSERT_MESSAGE(0, string().Format("ServerRecevedAwardsWorkingFromPlayer() player=%s; recieved bad award out of range=%d", GetEntityName(playerEntityId), workingValues[i].m_award));
				DbgLog("ServerRecevedAwardsWorkingFromPlayer() player=%s; recieved bad award out of range=%d", GetEntityName(playerEntityId), workingValues[i].m_award);
			}
		}

		awards->m_awardsReceivedFromClient=true;
	}
}

bool CAfterMatchAwards::IsAwardProhibited(EAfterMatchAwards inAward) const
{
	// Currently only have one prohibited award to worry about
	EGameMode mode = g_pGame->GetGameRules()->GetGameMode();
	if(mode == eGM_Gladiator && inAward == EAMA_Ninja)
	{
		return true;
	}

	return false;
}

void CAfterMatchAwards::FilterWinningAwards()
{
	float timeAlive = (float)m_persistantStats->GetDerivedStatForActorThisSession(EDIPS_AliveTime, m_localClientEntityIdWas);
	DbgLog("CAfterMatchAwards::FilterWinningAwards() localClient was alive for %f seconds", timeAlive);

	SAwardsForPlayer *awards = GetAwardsForActor(m_localClientEntityIdWas);
	if (awards)
	{
		DbgLog("CAfterMatchAwards::FilterWinningAwards() removing any awards we didn't stay alive long enough for to deserve");

		int len=awards->m_awards.size();
		for (int i=0; i<len; i++)
		{
			if (GetFlagsForAward(awards->m_awards[i]) & eAF_PlayerAliveTimeFloatMinLimit)
			{
				float minTimeAlive = GetFloatDataForAward(awards->m_awards[i]);
				DbgLog("CAfterMatchAwards::FilterWinningAwards() has found won award=%s which has a min time alive requirement of %f; player was alive for %f",	GetNameForAward(awards->m_awards[i]), minTimeAlive, timeAlive);

				if (timeAlive < minTimeAlive)
				{
					DbgLog("CAfterMatchAwards::FilterWinningAwards() has found player hasn't been alive long enough=%f for won award=%s which has a min time alive requirement of %f - removing from won list", timeAlive, GetNameForAward(awards->m_awards[i]), minTimeAlive);
					awards->m_awards.removeAt(i);
					i--;
					len--;
				}
			}

		}

		int originalNumAwards=awards->m_awards.size();

		// filter out all awards already earned
		CryFixedArray<EAfterMatchAwards, CAfterMatchAwards::kMaxLocalAwardsGiven> newlyWonAwards;

		len = awards->m_awards.size();
		for( int i = 0; i < len; i++ )
		{
			int style = 0;
			DbgLog("CAfterMatchAwards::FilterWinningAwards() has found that award=%s", GetNameForAward(awards->m_awards[i]));

			if(IsAwardProhibited(awards->m_awards[i]))
			{
				DbgLog("CAfterMatchAwards::FilterWinningAwards() has found that award=%s is currently prohibited", GetNameForAward(awards->m_awards[i]));
			}
			else
			{
				DbgLog("CAfterMatchAwards::FilterWinningAwards() has found that award=%s; has NOT been won before - this is a newly won award. Yay.", GetNameForAward(awards->m_awards[i]));
				newlyWonAwards.push_back(awards->m_awards[i]);
				awards->m_awards.removeAt(i);	// remove newly won awards to leave only already won awards in m_awards
				i--;
				len--;
			}
		}

		DbgLog("CAfterMatchAwards::FilterWinningAwards() had won %d awards; %d of these were newly won awards; %d were won before", originalNumAwards, newlyWonAwards.size(), awards->m_awards.size());

		if (newlyWonAwards.size() >= kMaxLocalAwardsActuallyWon)
		{
			DbgLog("CAfterMatchAwards::FilterWinningAwards() has newly won enough awards to make up the full kMaxLocalAwardsActuallyWon");

			// if there is enough newly won awards then these become the actual awards you've won
			awards->m_awards.clear();

			for (int i=0; i<kMaxLocalAwardsActuallyWon; i++)
			{
				uint32 randomIndex=cry_random(0U, newlyWonAwards.size() - 1);
				DbgLog("CAfterMatchAwards::FilterWinningAwards() randomly picked index=%d to give final winning award=%s", randomIndex, GetNameForAward(newlyWonAwards[randomIndex]));
				awards->m_awards.push_back(newlyWonAwards[randomIndex]);
				newlyWonAwards.removeAt(randomIndex);
			}
		}
		else
		{
			DbgLog("CAfterMatchAwards::FilterWinningAwards() has NOT newly won enough awards to make up the full kMaxLocalAwardsActuallyWon. Taking a mixture of awards");

			// there is not enough newly won awards so use all newly won and then pick the remaining from already won awards
			CryFixedArray<EAfterMatchAwards, CAfterMatchAwards::kMaxLocalAwardsActuallyWon> finalAwards;

			for (size_t i=0; i<newlyWonAwards.size(); i++)
			{
				DbgLog("CAfterMatchAwards::FilterWinningAwards() picking newly won award=%s", GetNameForAward(newlyWonAwards[i]));
				finalAwards.push_back(newlyWonAwards[i]);
			}

			size_t numRemaining = kMaxLocalAwardsActuallyWon - newlyWonAwards.size();
			DbgLog("CAfterMatchAwards::FilterWinningAwards() has %" PRISIZE_T " awards left to take from the already won awards (size=%d)", numRemaining, awards->m_awards.size());

			if (awards->m_awards.size() > numRemaining)
			{
				DbgLog("CAfterMatchAwards::FilterWinningAwards() has enough already won awards to satisfy numRemaining");
			}
			else
			{
				DbgLog("CAfterMatchAwards::FilterWinningAwards() has NOT got enough already won awards to satisfy numRemaining - using as many already won awards as we have");
				numRemaining=awards->m_awards.size();
			}

			size_t numRandomRequired = numRemaining;
			while (numRandomRequired > 0 && awards->m_awards.size())
			{
				uint32 randomIndex=cry_random(0U, awards->m_awards.size() - 1);
				EAfterMatchAwards award = awards->m_awards[randomIndex];

				if(!IsAwardProhibited(award))
				{
					DbgLog("CAfterMatchAwards::FilterWinningAwards() randomly picked index=%d to give final winning award=%s", randomIndex, GetNameForAward(award));
					finalAwards.push_back(award);
					--numRandomRequired; 
				}
				awards->m_awards.removeAt(randomIndex);
			}

			awards->m_awards.clear();

			for (size_t i=0; i<finalAwards.size(); i++)
			{
				awards->m_awards.push_back(finalAwards[i]);
			}
		}
	}


	DbgLog("CAfterMatchAwards::FilterWinningAwards() debugging awards after filtering");
	DebugMe(true);
}

void CAfterMatchAwards::HaveGotAwards()
{
	DbgLog("CAfterMatchAwards::HaveGotAwards()");

	m_state=k_state_game_ended_have_awards;
	DebugMe();

	CRY_ASSERT_MESSAGE(m_localClientEntityIdWas, "HaveGotAwards() failed, no local client entity set");

	SAwardsForPlayer *awards = GetAwardsForActor(m_localClientEntityIdWas);
	CRY_ASSERT_MESSAGE(awards, "HaveGotAwards() failed to find awards for localClient");
	if (awards)
	{
		FilterWinningAwards();
	}
}

void CAfterMatchAwards::DebugMe(bool useCryLog/*=false*/)
{
	if (useCryLog)
	{
		CryLog("CAfterMatchAwards::DebugMe()");
	}
	else
	{
		DbgLog("CAfterMatchAwards::DebugMe()");
	}


	// iterate awards and output those that are set
	
	ActorAwardsMap::const_iterator it = m_actorAwards.begin();
	ActorAwardsMap::const_iterator end = m_actorAwards.end();
	for ( ; it!=end; ++it)
	{
		const int len = it->second.m_awards.size();

		for (int i=0; i<len; i++)
		{
			if (useCryLog)
			{
				CryLog("Actor=%d; (%s) - Award=%s", it->first, GetEntityName(it->first), GetNameForAward(it->second.m_awards[i]));
			}
			else
			{
				DbgLog("Actor=%d; (%s) - Award=%s", it->first, GetEntityName(it->first), GetNameForAward(it->second.m_awards[i]));
			}
		}
	}
}

const char *CAfterMatchAwards::GetNameForAward(EAfterMatchAwards inAward)
{
	return s_afterMatchAwardsNames[inAward];
}

int CAfterMatchAwards::GetFlagsForAward(EAfterMatchAwards inAward)
{
	return s_awardFlags[inAward];
}

float CAfterMatchAwards::GetFloatDataForAward(EAfterMatchAwards inAward)
{
	return s_awardFloatData[inAward];
}

const char *CAfterMatchAwards::GetEntityName(EntityId inEntityId) const
{
	IEntity *entity = gEnv->pEntitySystem->GetEntity(inEntityId);
	if (entity)
	{
		return entity->GetName();
	}
	
	return "<NULL>";
}

// play nice with selotaped compiling
#undef DbgLog
#undef DbgLogAlways 
