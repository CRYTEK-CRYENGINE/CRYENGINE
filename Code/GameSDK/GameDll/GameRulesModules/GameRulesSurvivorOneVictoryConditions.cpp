// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

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

#include "StdAfx.h"
#include "GameRulesSurvivorOneVictoryConditions.h"

#if SURVIVOR_ONE_ENABLED

#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "IGameRulesPlayerStatsModule.h"
#include "IGameRulesStateModule.h"
#include "UI/UIManager.h"
#include "Audio/Announcer.h"
#include "Audio/AudioSignalPlayer.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "GameRulesModules/IGameRulesScoringModule.h"
#include "Utility/CryWatch.h"
#include "Utility/CryDebugLog.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"

#include "UI/HUD/HUDEventDispatcher.h"

#define DbgLog(...) CRY_DEBUG_LOG(GAMEMODE_ALLORNOTHING, __VA_ARGS__)
#define DbgLogAlways(...) CRY_DEBUG_LOG_ALWAYS(GAMEMODE_ALLORNOTHING, __VA_ARGS__)

#if CRY_WATCH_ENABLED
#define WATCH_LV1  g_pGameCVars->g_SurvivorOneVictoryConditions_watchLvl<1 ? (NULL) : CryWatch
#define WATCH_LV2  g_pGameCVars->g_SurvivorOneVictoryConditions_watchLvl<2 ? (NULL) : CryWatch
#else
#define WATCH_LV1
#define WATCH_LV2
#endif


#define WATCH_LATEST_SURVIVOR_COUNT  (1 && CRY_WATCH_ENABLED)

//-------------------------------------------------------------------------
CGameRulesSurvivorOneVictoryConditions::CGameRulesSurvivorOneVictoryConditions()
{
	if (gEnv->bServer)
	{
		m_svLatestSurvCount = -123;
		memset(m_svLatestSurvList, 0, (CRY_ARRAY_COUNT(m_svLatestSurvList)));
	}

	m_radarPingArray.push_back();
	m_radarPingArray.push_back();
	m_radarPingArray.push_back();
}

//-------------------------------------------------------------------------
CGameRulesSurvivorOneVictoryConditions::~CGameRulesSurvivorOneVictoryConditions()
{
	CGameRules*  pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->UnRegisterRoundsListener(this);

		if (gEnv->bServer)
		{
			pGameRules->UnRegisterSurvivorCountListener(this);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesSurvivorOneVictoryConditions::Init( XmlNodeRef xml )
{
	inherited::Init(xml);

	int  dbgWatchLvl;
	if (xml->getAttr("dbgWatchLvl", dbgWatchLvl))
	{
		g_pGameCVars->g_SurvivorOneVictoryConditions_watchLvl = dbgWatchLvl;
	}

	g_pGameCVars->g_mpAllSeeingRadar=0;

	float tmpFloat;
	if (xml->getAttr("radarStage0TimeLimit", tmpFloat))
	{
		m_radarPingArray[0].timeLimit = tmpFloat;
	}
	if (xml->getAttr("radarStage0PingTime", tmpFloat))
	{
		m_radarPingArray[0].pingTime = tmpFloat;
	}

	if (xml->getAttr("radarStage1TimeLimit", tmpFloat))
	{
		m_radarPingArray[1].timeLimit = tmpFloat;
	}
	if (xml->getAttr("radarStage1PingTime", tmpFloat))
	{
		m_radarPingArray[1].pingTime = tmpFloat;
	}

	if (xml->getAttr("radarStage2TimeLimit", tmpFloat))
	{
		m_radarPingArray[2].timeLimit = tmpFloat;
	}
	if (xml->getAttr("radarStage2PingTime", tmpFloat))
	{
		m_radarPingArray[2].pingTime = tmpFloat;
	}

	m_scoreIncreasePerElimination=0;
	int tmpInt;
	if (xml->getAttr("scoreIncreasePerElimination", tmpInt))
	{
		m_scoreIncreasePerElimination=tmpInt;
	}


	m_radarPingStage=0;
	m_radarPingStageTimer=m_radarPingArray[0].timeLimit;
	m_radarPingTimer=m_radarPingArray[0].pingTime;

	CGameRules*  pGameRules = g_pGame->GetGameRules();
	if (pGameRules)
	{
		pGameRules->RegisterRoundsListener(this);
	}

	if (gEnv->bServer)
	{
		if (pGameRules)
		{
			pGameRules->RegisterSurvivorCountListener(this);
		}
		m_svLatestSurvCount = -123;
		memset(m_svLatestSurvList, 0, (CRY_ARRAY_COUNT(m_svLatestSurvList)));
	}
}

void CGameRulesSurvivorOneVictoryConditions::UpdateRadarPings(float frameTime)
{
	if (!gEnv->IsClient())
	{
		return;
	}

	// only do any of this logic when in game, not in pregame or any other state
	// we're getting close to going into stage 1 before the game starts
	// now we're driven off of gametime
	IGameRulesStateModule *pStateModule = g_pGame->GetGameRules()->GetStateModule();
	if (pStateModule != NULL && pStateModule->GetGameState() != IGameRulesStateModule::EGRS_InGame)
	{
		return;
	}

	float currentGameTime = m_pGameRules->GetCurrentGameTime();
	WATCH_LV1("gameTime=%f", currentGameTime);
	float currentStageTimeLimit=-1.f;
	if (m_radarPingStage < m_radarPingArray.size())
	{
		currentStageTimeLimit = m_radarPingArray[m_radarPingStage].timeLimit;
	}
	WATCH_LV1("radarPing: stage=%d; stageTimeLimit=%f; pingTimer=%f", m_radarPingStage, currentStageTimeLimit, m_radarPingTimer);

	/*
	m_radarPingStageTimer -= frameTime;
	if (m_radarPingStageTimer <= 0.f)
	{
		DbgLog("CGameRulesSurvivorOneVictoryConditions::UpdateRadarPings() - radarPingStageTimer expired trying to move to next stage");
		m_radarPingStage++;

		if (m_radarPingStage < m_radarPingArray.size())
		{
			m_radarPingStageTimer = m_radarPingArray[m_radarPingStage].timeLimit;
		}
	}
	*/
	
	if (m_radarPingStage < m_radarPingArray.size())
	{
		if (currentGameTime > m_radarPingArray[m_radarPingStage].timeLimit)
		{
			DbgLog("CGameRulesSurvivorOneVictoryConditions::UpdateRadarPings() - gameTime is > stageTimer trying to move onto the next stage");
			m_radarPingStage++;
		}
	}

	if (m_radarPingStage < m_radarPingArray.size())
	{
		if (m_radarPingArray[m_radarPingStage].pingTime > 0.f)
		{
			// this stage has a valid pingtime
			m_radarPingTimer -= frameTime;
			if (m_radarPingTimer <= 0.f)
			{
				DbgLog("CGameRulesSurvivorOneVictoryConditions::UpdateRadarPings() - radarPingTimer expired performing a radar ping on all survivors");

				EntityId localPlayerId = g_pGame->GetIGameFramework()->GetClientActorId();
				CAudioSignalPlayer::JustPlay("AllOrNothingRadarPulse", localPlayerId);

				IActorIteratorPtr pActorIterator = g_pGame->GetIGameFramework()->GetIActorSystem()->CreateActorIterator();
				while (IActor* pActor = pActorIterator->Next())
				{
					EntityId actorEntityId = pActor->GetEntityId();
					if (actorEntityId != localPlayerId && pActor->IsPlayer() && pActor->GetChannelId() && !pActor->IsDead())
					{
						SHUDEvent eventTempAddToRadar(eHUDEvent_TemporarilyTrackEntity);
						eventTempAddToRadar.AddData( static_cast<int>(actorEntityId) );
						eventTempAddToRadar.AddData( 5.f ); // Time to stay on radar
						eventTempAddToRadar.AddData( true ); // true = don't show tagname as well
						eventTempAddToRadar.AddData( true ); // force no updating pos
						CHUDEventDispatcher::CallEvent(eventTempAddToRadar);					
					}
				}

				m_radarPingTimer = m_radarPingArray[m_radarPingStage].pingTime;
			}
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesSurvivorOneVictoryConditions::Update( float frameTime )
{
	inherited::Update(frameTime);

	UpdateRadarPings(frameTime);

#if !defined(_RELEASE)
	IGameRulesScoringModule* pScoringModule=g_pGame->GetGameRules()->GetScoringModule();
	if (pScoringModule)
	{
		TGameRulesScoreInt currentDeathScoringModifier = pScoringModule->GetDeathScoringModifier();
		CryWatch("deathScoreModifier=%d", currentDeathScoringModifier);
	}
#endif

	if (!gEnv->bServer)
		return;

	WATCH_LV2("[CGameRulesSurvivorOneVictoryConditions]");

#if WATCH_LATEST_SURVIVOR_COUNT
	WATCH_LV2("Survivors (%d) :", m_svLatestSurvCount);
	for (int i=0; i<m_svLatestSurvCount; i++)
	{
		IEntity*  e = gEnv->pEntitySystem->GetEntity(m_svLatestSurvList[i]);
		WATCH_LV2(" %s", (e?e->GetName():"!"));
	}
#endif



	/*
	IGameRulesStateModule*  pStateModule = m_pGameRules->GetStateModule();
	if (pStateModule && pStateModule->GetGameState() != IGameRulesStateModule::EGRS_InGame)
		return;
	*/
}

//-------------------------------------------------------------------------
void CGameRulesSurvivorOneVictoryConditions::ClVictoryPlayer( int playerId, EGameOverReason reason, const EntityId killedEntity, const EntityId shooterEntity )
{
	inherited::ClVictoryPlayer(playerId, reason, killedEntity, shooterEntity);
}

//-------------------------------------------------------------------------
void CGameRulesSurvivorOneVictoryConditions::TimeLimitExpired()
{
	if (gEnv->bServer)  // this might always be true anyway, dunno
	{
		IGameRulesRoundsModule*  pRoundsModule = m_pGameRules->GetRoundsModule();
	
		if (pRoundsModule != NULL && !pRoundsModule->IsInSuddenDeath() && pRoundsModule->CanEnterSuddenDeath())
		{
			DbgLog("CGameRulesSurvivorOneVictoryConditions::TimeLimitExpired() - We're going into sudden death!");
			pRoundsModule->OnEnterSuddenDeath();
	
			DbgLog("CGameRulesSurvivorOneVictoryConditions::TimeLimitExpired() - turning on all seeing radar cvar");
			// Allow all players to see all other players on the radar
			// server controlled doesn't work well for late joiners, migration etc
			// g_pGameCVars->g_mpAllSeeingRadarSv=1;
			return;
		}

		if (!pRoundsModule || !pRoundsModule->IsRestarting())
		{
			assert(m_svLatestSurvCount > 1);
			if (IGameRulesScoringModule* pScoringModule=g_pGame->GetGameRules()->GetScoringModule())
			{
				for (int i=0; i<m_svLatestSurvCount; i++)
				{
					CCCPOINT(SurvivorOneVictoryConditions_SvDrawScoreEvent);
					pScoringModule->OnPlayerScoringEvent(m_svLatestSurvList[i], EGRST_AON_Draw);
				}
			}
			CCCPOINT(SurvivorOneVictoryConditions_SvTimeLimitReached);
			OnEndGamePlayer(0, EGOR_TimeLimitReached);
		}
	}
}

//-------------------------------------------------------------------------
void CGameRulesSurvivorOneVictoryConditions::OnEndGamePlayer( EntityId playerId, EGameOverReason reason )
{
	inherited::OnEndGamePlayer(playerId, reason);
	
	DbgLog("CGameRulesSurvivorOneVictoryConditions::OnEndGamePlayer() - turning off all seeing radar cvar");
	
	// Stop all players to see all other players on the radar
	// server controlled doesn't work well for late joiners, migration etc
	//g_pGameCVars->g_mpAllSeeingRadarSv=1;
}

//-------------------------------------------------------------------------
// IGameRulesSurvivorCountListener
void CGameRulesSurvivorOneVictoryConditions::SvSurvivorCountRefresh(int count, const EntityId survivors[], int numKills)
{
	CRY_ASSERT(gEnv->bServer);

	IGameRulesScoringModule* pScoringModule=g_pGame->GetGameRules()->GetScoringModule();
	if (count != m_svLatestSurvCount && pScoringModule != NULL)
	{
		DbgLog("CGameRulesSurvivorOneVictoryConditions::SvSurvivorCountRefresh() survivor count updating from %d to %d", m_svLatestSurvCount, count);

		if (count < m_svLatestSurvCount) // should always be the case 
		{
			int survivorDiff=m_svLatestSurvCount-count;
			TGameRulesScoreInt currentDeathScoringModifier = pScoringModule->GetDeathScoringModifier();
			TGameRulesScoreInt newDeathScoringModifier=currentDeathScoringModifier + (survivorDiff*m_scoreIncreasePerElimination);

			DbgLog("CGameRulesSurvivorOneVictoryConditions::SvSurvivorCountRefresh() death scoring modifier going from %d to %d from a survivor delta of %d", currentDeathScoringModifier, newDeathScoringModifier, survivorDiff);
			pScoringModule->SvSetDeathScoringModifier(newDeathScoringModifier);
		}
	}

	int  sz = (count * sizeof(survivors[0]));
	CRY_ASSERT_MESSAGE(sz <= sizeof(m_svLatestSurvList), "Memory overwrite!");
	memcpy(m_svLatestSurvList, survivors, sz);
	m_svLatestSurvCount = count;

	IGameRulesStateModule*  pStateModule = m_pGameRules->GetStateModule();
	if (pStateModule && (pStateModule->GetGameState() == IGameRulesStateModule::EGRS_InGame))
	{
		if (count == 1)  // TODO could also pass a count delta into this function (the amount of change since this function was last called) to check here whether or not the count has actually gone *down* to 1
		{
			if (numKills > 0)
			{
				EntityId  winner = survivors[0];
				CRY_ASSERT(winner);
				if (pScoringModule)
				{
					CCCPOINT(SurvivorOneVictoryConditions_SvSurvivorOneScoreEvent);
					pScoringModule->OnPlayerScoringEvent(winner, EGRST_AON_Win);
				}
				CCCPOINT(SurvivorOneVictoryConditions_SvSurvivorOneEndGame);
				OnEndGamePlayer(winner, EGOR_ObjectivesCompleted);  // TODO do we need our own EGOR_ value here?
			}
		}
	}
}

//-------------------------------------------------------------------------
// IGameRulesRoundsListener
void CGameRulesSurvivorOneVictoryConditions::OnRoundStart()
{
	DbgLog("CGameRulesSurvivorOneVictoryConditions::OnRoundStart()");

	m_radarPingStage=0;
	m_radarPingTimer=m_radarPingArray[0].pingTime;

	IGameRulesScoringModule* pScoringModule=g_pGame->GetGameRules()->GetScoringModule();
	if (pScoringModule)
	{
		DbgLog("CGameRulesSurvivorOneVictoryConditions::OnRoundStart() resetting the death scoring modifier to 0");
		if (gEnv->bServer)
		{
			pScoringModule->SvSetDeathScoringModifier(0);
		}
	}
}

//-------------------------------------------------------------------------
// IGameRulesRoundsListener
void CGameRulesSurvivorOneVictoryConditions::OnRoundEnd()
{
	DbgLog("CGameRulesSurvivorOneVictoryConditions::OnRoundEnd() - stopping showing all on radar");

	g_pGameCVars->g_mpAllSeeingRadar=0;
}

//-------------------------------------------------------------------------
// IGameRulesRoundsListener
void CGameRulesSurvivorOneVictoryConditions::OnSuddenDeath()
{
	DbgLog("CGameRulesSurvivorOneVictoryConditions::OnSuddenDeath() - showing all on radar");

	g_pGameCVars->g_mpAllSeeingRadar=1;
}

// play nice with selotaped compiling
#undef DbgLog
#undef DbgLogAlways 

#undef WATCH_LV1
#undef WATCH_LV2

#endif // SURVIVOR_ONE_ENABLED
