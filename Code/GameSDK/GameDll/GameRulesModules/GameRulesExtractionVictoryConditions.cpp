// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Game rules module to handle victory conditions for extraction game mode
	-------------------------------------------------------------------------
	History:
	- 14/05/2009  : Created by Jim Bamford

*************************************************************************/

#include "StdAfx.h"
#include "GameRulesExtractionVictoryConditions.h"
#include "GameRules.h"
#include "IGameRulesStateModule.h"
#include "IGameRulesObjectivesModule.h"
#include "IGameRulesRoundsModule.h"
#include "Utility/CryDebugLog.h"
#include "GameRulesTypes.h"

#define DbgLog(...) CRY_DEBUG_LOG(GAMEMODE_EXTRACTION, __VA_ARGS__)
#define DbgLogAlways(...) CRY_DEBUG_LOG_ALWAYS(GAMEMODE_EXTRACTION, __VA_ARGS__)

//-------------------------------------------------------------------------
void CGameRulesExtractionVictoryConditions::Update( float frameTime )
{
	IGameRulesStateModule *pStateModule = m_pGameRules->GetStateModule();
	if (pStateModule != NULL && pStateModule->GetGameState() != IGameRulesStateModule::EGRS_InGame)
		return;

	IGameRulesRoundsModule* pRoundsModule=m_pGameRules->GetRoundsModule();
	if (pRoundsModule != NULL && !pRoundsModule->IsInProgress())
	{
		return;
	}

	UpdateTimeLimitSounds();

	if (!gEnv->bServer)
		return;

	DebugDrawResolutions();
	SvCheckOpponents();
	CheckTimeLimit();
}

void CGameRulesExtractionVictoryConditions::TimeLimitExpired()
{
	CRY_ASSERT(gEnv->bServer);

	IGameRulesRoundsModule*  pRoundsModule = m_pGameRules->GetRoundsModule();
	CRY_ASSERT(pRoundsModule);

	if (pRoundsModule)
	{
		if (!pRoundsModule->IsInSuddenDeath() && pRoundsModule->CanEnterSuddenDeath())
		{
			DbgLog("CGameRulesExtractionVictoryConditions::TimeLimitExpired() - We're going into sudden death!");
			pRoundsModule->OnEnterSuddenDeath();
			return;
		}

		int  primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking
		int secondaryTeam = (primaryTeam == 1) ? 2 : 1;			// defending

		DbgLog("CGameRulesExtractionVictoryConditions::TimeLimitExpired() - secondary team %d has won", secondaryTeam);

		//		notify listeners
		if (pRoundsModule->IsInProgress())
			m_pGameRules->SvOnTimeLimitExpired_NotifyListeners();  // nb. must come before the call to GetMaxTeamScore(), because listeners of this notification might want to award scores which then need to be taken into account by GetMaxTeamScore()

		//		and award win for secondary team
		OnEndGame(0, EGOR_TimeLimitReached);
	}
}

//-------------------------------------------------------------------------
void CGameRulesExtractionVictoryConditions::TeamCompletedAnObjective(int teamId)
{
	CGameRules *pGameRules = g_pGame->GetGameRules();
	IGameRulesRoundsModule*  pRoundsModule = pGameRules->GetRoundsModule();
	CRY_ASSERT(pRoundsModule);

	if (pRoundsModule)
	{
		int  primaryTeam = pRoundsModule->GetPrimaryTeam();	// attacking

		DbgLog("CGameRulesExtractionVictoryConditions::TeamCompletedAnObjective() primary team %d has completed an objective", primaryTeam);
		UpdateResolutionData(primaryTeam);

		int roundNumber = pRoundsModule->GetRoundNumber();
		CryLog("CGameRulesExtractionVictoryConditions::TeamCompletedAnObjective() roundNumber=%d", roundNumber);

		if (roundNumber == 1)
		{
			// only in round 2 can the attackers win early by capturing more than the attackers did last round

			int secondaryTeam = (primaryTeam == 1) ? 2 : 1;			// defending

			int primaryScore = pGameRules->GetTeamRoundScore(primaryTeam);
			int secondaryScore = pGameRules->GetTeamRoundScore(secondaryTeam);

			CryLog("CGameRulesExtractionVictoryConditions::TeamCompletedAnObjective() we're in round two... team scores primary=%d; secondary=%d", primaryScore, secondaryScore);
			if (primaryScore > secondaryScore)
			{
				CryLog("CGameRulesExtractionVictoryConditions::TeamCompletedAnObjective() the attacking team has captured more ticks in the second round than the attackers did in the first round... they win!");
				OnEndGame(primaryTeam, EGOR_ObjectivesCompleted);
			}
		}
	}
}

void CGameRulesExtractionVictoryConditions::UpdateResolutionData(int primaryTeam)
{
	DbgLog("CGameRulesExtractionVictoryConditions::UpdateResolutionData() primary team %d has won", primaryTeam);

	float timeTaken = m_pGameRules->GetCurrentGameTime();
	float team1Data=0.f;
	float team2Data=0.f;

	if (primaryTeam == 1)
	{
		team1Data = timeTaken;
	}
	else if (primaryTeam == 2)
	{
		team2Data = timeTaken;
	}
	else
	{
		CRY_ASSERT_MESSAGE(0, "UpdateResolutionData() unexpected primaryTeam");
	}

	MaxFloatsToDrawResolutionData(ESVC_DrawResolution_level_1, team1Data, team2Data);
}

// play nice with selotaped compiling
#undef DbgLog
#undef DbgLogAlways 