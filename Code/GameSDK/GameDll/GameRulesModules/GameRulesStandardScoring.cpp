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

#include "StdAfx.h"
#include "GameRulesStandardScoring.h"
#include <CrySystem/XML/IXml.h>
#include "GameRules.h"
#include "IGameRulesPlayerStatsModule.h"
#include "IGameRulesStateModule.h"
#include "GameRulesTypes.h"
#include "Utility/DesignerWarning.h"
#include "Utility/CryDebugLog.h"
#include "Player.h"
#include "StatsRecordingMgr.h"

AUTOENUM_BUILDNAMEARRAY(CGameRulesStandardScoring::s_gamerulesScoreType, EGRSTList);

#define STANDARD_SCORING_STATE_ASPECT		eEA_GameServerA

//-------------------------------------------------------------------------
CGameRulesStandardScoring::CGameRulesStandardScoring()
{
	m_maxTeamScore = 0;
	m_startTeamScore = 0;
	m_useScoreAsTime = false;
	m_bAttackingTeamWonAllRounds = true;
	m_deathScoringModifier = 0;

	for(int i = 0; i < EGRST_Num; i++)
	{
		m_playerScorePoints[i] = 0;
		m_playerScoreXP[i] = 0;
		m_teamScorePoints[i] = 0;
	}
}

//-------------------------------------------------------------------------
CGameRulesStandardScoring::~CGameRulesStandardScoring()
{
}

//-------------------------------------------------------------------------
void CGameRulesStandardScoring::Init( XmlNodeRef xml )
{
	int numScoreCategories = xml->getChildCount();
	for (int i = 0; i < numScoreCategories; ++ i)
	{
		XmlNodeRef categoryXml = xml->getChild(i);
		const char *categoryTag = categoryXml->getTag();

		if (!stricmp(categoryTag, "Player"))
		{
			InitScoreData(categoryXml, &m_playerScorePoints[0], m_playerScoreXP);
		}
		else if (!stricmp(categoryTag, "Team"))
		{
			InitScoreData(categoryXml, &m_teamScorePoints[0], NULL);
			categoryXml->getAttr("maxScore", m_maxTeamScore);
			categoryXml->getAttr("startTeamScore", m_startTeamScore);
			categoryXml->getAttr("useScoreAsTime", m_useScoreAsTime);
		}
	}
}

bool CGameRulesStandardScoring::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if (aspect == STANDARD_SCORING_STATE_ASPECT)
	{
		int deathScoringModifierWas=m_deathScoringModifier;
		ser.Value("deathScoreModifier", m_deathScoringModifier, 'ui10'); // max 1023
	}

	return true;
}

void CGameRulesStandardScoring::InitScoreData(XmlNodeRef categoryXml, TGameRulesScoreInt *scoringData, TGameRulesScoreInt *xpData)
{
	CRY_ASSERT(scoringData);

	int numScoreValues = categoryXml->getChildCount();
	for (int j = 0; j < numScoreValues; ++ j)
	{
		XmlNodeRef childXml = categoryXml->getChild(j);
		const char *scoringTag = childXml->getTag();

		if (!stricmp(scoringTag, "Event"))
		{
			int points = 0;
			if (childXml->getAttr("points", points))
			{
				int type = EGRST_Unknown;
				const char*  pChar = NULL;
				if (childXml->getAttr("type", &pChar))
				{
					bool  typeOk = AutoEnum_GetEnumValFromString(pChar, s_gamerulesScoreType, EGRST_Num, &type);
					if (typeOk)
					{
						DesignerWarning( points < SGameRulesScoreInfo::SCORE_MAX && points > SGameRulesScoreInfo::SCORE_MIN, "Adding score for player which is out of net-serialize bounds (%d is not within [%d .. %d])", points, SGameRulesScoreInfo::SCORE_MIN, SGameRulesScoreInfo::SCORE_MAX );
						scoringData[type] = static_cast<TGameRulesScoreInt>(points);

						if (xpData)
						{
							int xp = points;		// Default XP to be the same as points if not specified
							childXml->getAttr("xp", xp);

							xpData[type] = static_cast<TGameRulesScoreInt>(xp);
						}
					}
					else
					{
						CryLogAlways("GameRulesStandardScoring::Init() : Scoring Event type not recognised: %s.", pChar);
					}
				}
				else
				{
					CryLogAlways("GameRulesStandardScoring::Init() : Scoring Event has no type declared.");
				}
			}
			else
			{
				CryLogAlways("GameRulesStandardScoring::Init() : Scoring Event has no points declared.");
			}
		}
	}
}

//-------------------------------------------------------------------------
TGameRulesScoreInt CGameRulesStandardScoring::GetPlayerPointsByType(EGRST pointsType) const
{
	return GetPointsByType(&m_playerScorePoints[0], pointsType);
}

//-------------------------------------------------------------------------
TGameRulesScoreInt CGameRulesStandardScoring::GetPlayerXPByType(EGRST pointsType) const
{
	return GetPointsByType(m_playerScoreXP, pointsType);
}

//-------------------------------------------------------------------------
TGameRulesScoreInt CGameRulesStandardScoring::GetTeamPointsByType(EGRST pointsType) const
{
	return GetPointsByType(&m_teamScorePoints[0], pointsType);
}

//-------------------------------------------------------------------------
TGameRulesScoreInt CGameRulesStandardScoring::GetPointsByType(const TGameRulesScoreInt *scoringData, EGRST pointsType) const
{
	CRY_ASSERT_MESSAGE(pointsType > EGRST_Unknown && pointsType < EGRST_Num, "Out of range parameter passed into CGameRulesStandardScoring::GetPointsByType");
	const TGameRulesScoreInt &scoreData = scoringData[pointsType];

	if(scoreData == 0)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_COMMENT, "Scoring not setup for %s in gamemode %s", s_gamerulesScoreType[pointsType], gEnv->pConsole->GetCVar("sv_gamerules")->GetString());
	}

	return scoreData;
}

void CGameRulesStandardScoring::DoScoringForDeath(IActor *pTargetActor, EntityId shooterId, int damage, int material, int hit_type)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IActor *pShooterActor =  g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(shooterId);

	if (pGameRules != NULL && pTargetActor != NULL && pShooterActor != NULL)
	{
		if (pGameRules->HasGameActuallyStarted() == false)
		{
			return;
		}

		// No scoring at game end
		IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
		if (pStateModule != NULL && pStateModule->GetGameState() == IGameRulesStateModule::EGRS_PostGame)
		{
			return;
		}

		EntityId targetId = pTargetActor->GetEntityId();
		bool bTeamGame = (pGameRules->GetTeamCount() > 1);
		int targetTeam = pGameRules->GetTeam(targetId);
		int shooterTeam = pGameRules->GetTeam(shooterId);
		
		int playerPoints = 0, playerXP = 0;
		EGRST scoreType;

		bool useModifier=true;
		if (pTargetActor == pShooterActor)
		{
			scoreType = EGRST_Suicide;
			useModifier = false;
		}
		else if (bTeamGame && (targetTeam == shooterTeam))
		{
			scoreType = EGRST_PlayerTeamKill;
		}
		else
		{
			scoreType = EGRST_PlayerKill;
		}

		// map reason from one enum to other
		EXPReason			reason=EXPReasonFromEGRST(scoreType);

		playerPoints = GetPlayerPointsByType(scoreType);
		playerXP = GetPlayerXPByType(scoreType);
		
		if (useModifier)
		{
			playerPoints += m_deathScoringModifier;
			playerXP += m_deathScoringModifier;
		}

		if(pShooterActor->IsPlayer())
		{
			playerXP = static_cast<CPlayer*>(pShooterActor)->GetXPBonusModifiedXP(playerXP);
		}

		SGameRulesScoreInfo scoreInfo((EGameRulesScoreType) scoreType, playerPoints, playerXP, reason);
		scoreInfo.AttachVictim(targetId);
		CryLog("About to call pGameRules->IncreasePoints, pGameRules=%p", pGameRules);
		INDENT_LOG_DURING_SCOPE();

		OnTeamScoringEvent(shooterTeam, scoreType);
		pGameRules->IncreasePoints(shooterId, scoreInfo);
	}
}

bool CGameRulesStandardScoring::ShouldScore(CGameRules *pGameRules) const
{
	if (!pGameRules)
		return false;

	if (pGameRules->HasGameActuallyStarted() == false)
	{
		return false;
	}

	// No scoring at game end
	IGameRulesStateModule *pStateModule = pGameRules->GetStateModule();
	if (pStateModule != NULL && pStateModule->GetGameState() == IGameRulesStateModule::EGRS_PostGame)
	{
		return false;
	}

	return true;
}

void CGameRulesStandardScoring::OnPlayerScoringEvent( EntityId playerId, EGRST pointsType)
{
	CRY_ASSERT(pointsType != EGRST_Unknown);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(ShouldScore(pGameRules))
	{
		int playerPoints = GetPlayerPointsByType(pointsType);
		int playerXP = GetPlayerXPByType(pointsType);
		
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(playerId);
		if(pActor && pActor->IsPlayer())
		{
			playerXP = static_cast<CPlayer*>(pActor)->GetXPBonusModifiedXP(playerXP);
		}

		if(playerPoints != 0)
		{
			EXPReason		reason=EXPReasonFromEGRST(pointsType);
			SGameRulesScoreInfo  scoreInfo(pointsType, playerPoints, playerXP, reason);
			pGameRules->IncreasePoints(playerId, scoreInfo);
		}
	}
}

void CGameRulesStandardScoring::OnPlayerScoringEventWithInfo(EntityId playerId, SGameRulesScoreInfo* scoreInfo)
{
	CRY_ASSERT(scoreInfo);
	CRY_ASSERT(scoreInfo->type != EGRST_Unknown);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(ShouldScore(pGameRules))
	{
		if(scoreInfo->score != 0)
		{
			SGameRulesScoreInfo newScoreInfo(*scoreInfo);
			pGameRules->IncreasePoints(playerId, newScoreInfo);
		}
	}
}

void CGameRulesStandardScoring::OnPlayerScoringEventToAllTeamWithInfo(const int teamId, SGameRulesScoreInfo* scoreInfo)
{
	CRY_ASSERT(gEnv->bServer);

	CGameRules::TPlayers  teamPlayers;
	g_pGame->GetGameRules()->GetTeamPlayers(teamId, teamPlayers);
	
	CGameRules::TPlayers::const_iterator  it = teamPlayers.begin();
	CGameRules::TPlayers::const_iterator  end = teamPlayers.end();
	for (; it!=end; ++it)
	{
		CPlayer  *loopPlr = static_cast< CPlayer* >( gEnv->pGameFramework->GetIActorSystem()->GetActor(*it) );
		OnPlayerScoringEventWithInfo(loopPlr->GetEntityId(), scoreInfo);
	}
}

void CGameRulesStandardScoring::OnTeamScoringEvent( int teamId, EGRST pointsType)
{
	CRY_ASSERT(pointsType != EGRST_Unknown);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(ShouldScore(pGameRules))
	{
		bool bTeamGame = (pGameRules->GetTeamCount() > 1);
		if (bTeamGame)
		{
			int teamPoints = GetTeamPointsByType(pointsType);
			if (teamPoints)
			{
				int teamScore = pGameRules->GetTeamsScore(teamId) + teamPoints;

				if (m_maxTeamScore)
				{
					if (m_useScoreAsTime)
					{
						float gameTime = pGameRules->GetCurrentGameTime();
						int maxActualScore = (int)floor(gameTime + m_maxTeamScore);
						if (teamScore > maxActualScore)
						{
							teamScore = maxActualScore;
						}
					}
					else
					{
						if (teamScore > m_maxTeamScore)
						{
							teamScore = m_maxTeamScore;
						}
					}
				}
				pGameRules->SetTeamsScore(teamId, teamScore);

				CStatsRecordingMgr* pRecordingMgr = g_pGame->GetStatsRecorder();
				if (pRecordingMgr)
				{
					pRecordingMgr->OnTeamScore(teamId, teamPoints, pointsType);
				}
			}
		}
	}
}

void CGameRulesStandardScoring::SvResetTeamScore(int teamId)
{
	CRY_ASSERT(gEnv->bServer);

	CGameRules *pGameRules = g_pGame->GetGameRules();
	if(ShouldScore(pGameRules))
	{
		bool bTeamGame = (pGameRules->GetTeamCount() > 1);
		CRY_ASSERT_MESSAGE(bTeamGame, "we can't reset team score in a non-team game");
		if (bTeamGame)
		{
			pGameRules->SetTeamsScore(teamId, 0);
		}
	}
}

// IGameRulesScoringModule
void CGameRulesStandardScoring::SvSetDeathScoringModifier(TGameRulesScoreInt inModifier)
{
	CRY_ASSERT(gEnv->bServer);

	if (inModifier != m_deathScoringModifier)
	{
		m_deathScoringModifier = inModifier; 
		CGameRules *pGameRules = g_pGame->GetGameRules();
		CHANGED_NETWORK_STATE(pGameRules, STANDARD_SCORING_STATE_ASPECT);
	}
}
