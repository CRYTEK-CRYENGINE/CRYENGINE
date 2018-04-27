// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Game.h"
#include "IGameRulesPlayerStatsModule.h"
#include "GameRulesObjectiveVictoryConditionsIndividualScore.h"
#include "GameRules.h"
#include "IGameRulesStateModule.h"
#include "IGameRulesObjectivesModule.h"
#include "GameRulesModules/IGameRulesRoundsModule.h"
#include "PersistantStats.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "Audio/Announcer.h"
#include "Network/Lobby/GameLobby.h"


void CGameRulesObjectiveVictoryConditionsIndividualScore::OnEndGame(int teamId, EGameOverReason reason, ESVC_DrawResolution winningResolution, EntityId killedEntity, EntityId shooterEntity)
{
	EGameOverType gameOverType = EGOT_Unknown;

	if (gEnv->bServer)
	{
		IGameRulesRoundsModule *pRoundsModule = m_pGameRules->GetRoundsModule();
		if (pRoundsModule)
		{
			pRoundsModule->OnEndGame(teamId, 0, reason);

			if (!pRoundsModule->IsGameOver())
			{
				return;
			}
		}

		EntityId localClient = gEnv->pGameFramework->GetClientActorId();
		if ( shooterEntity == localClient && localClient != 0)
		{
			g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_WinningKill);
		}
		if ( killedEntity == localClient && localClient != 0)
		{
			g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_VictimOnFinalKillcam);
		}

		IGameRulesStateModule *pStateModule = m_pGameRules->GetStateModule();
		if (pStateModule)
			pStateModule->OnGameEnd();


		IGameRulesPlayerStatsModule *pStatsModule = m_pGameRules->GetPlayerStatsModule();
		if (pStatsModule)
		{
			pStatsModule->CalculateSkillRanking();
		}

		int team1Score = m_pGameRules->GetTeamsScore(1);
		int team2Score = m_pGameRules->GetTeamsScore(2);
		m_victoryParams = CGameRules::VictoryTeamParams(teamId, reason, team1Score, team2Score, winningResolution, m_drawResolutionData[ESVC_DrawResolution_level_1], m_drawResolutionData[ESVC_DrawResolution_level_2], m_winningVictim, m_winningAttacker);
		m_winningVictim = 0;
		m_winningAttacker = 0;
		m_pGameRules->GetGameObject()->InvokeRMI(CGameRules::ClVictoryTeam(), m_victoryParams, eRMI_ToRemoteClients);

		// have finished processing these winning stats now
		m_winningAttacker=0;
		m_winningVictim=0;
	}

	m_pGameRules->OnEndGame();

	int highestPlayerScore = -1;

	EntityId playerId = 0;
	bool bDraw = true;
	IGameRulesPlayerStatsModule* pPlayStatsMo = m_pGameRules->GetPlayerStatsModule();
	if (pPlayStatsMo)
	{
		const int numStats = pPlayStatsMo->GetNumPlayerStats();
		for (int i=0; i<numStats; i++)
		{
			const SGameRulesPlayerStat*  s = pPlayStatsMo->GetNthPlayerStats(i);
			if (s->flags & SGameRulesPlayerStat::PLYSTATFL_HASSPAWNEDTHISROUND)
			{
				if (s->points == highestPlayerScore)
				{
					bDraw = true;
					playerId = 0;
				}
				else if (s->points > highestPlayerScore)
				{
					bDraw = false;
					highestPlayerScore = s->points;
					playerId = s->playerId;
				}
			}
		}
	}

	if (gEnv->IsClient())
	{
		int clientScore = 0;
		bool clientScoreIsTop = false;
		const EntityId clientId = g_pGame->GetIGameFramework()->GetClientActorId();

		if (pPlayStatsMo)
		{
			if (const SGameRulesPlayerStat* s = pPlayStatsMo->GetPlayerStats(clientId))
			{
				clientScore = s->points;
			}

			if (clientScore == highestPlayerScore)
			{
				clientScoreIsTop = true;
			}
		}

		const bool localPlayerWon = (playerId == clientId);

		if (!bDraw)
		{
			if (localPlayerWon)
			{
				CAudioSignalPlayer::JustPlay("MatchWon");
			}
			else
			{
				CAudioSignalPlayer::JustPlay("MatchLost");
			}
		}
		else
		{
			CAudioSignalPlayer::JustPlay("MatchDraw");
		}

		// the server has already increased these stats in the server block above
		if (!gEnv->bServer)
		{
			EntityId localClient = gEnv->pGameFramework->GetClientActorId();
			if ( shooterEntity == localClient && localClient != 0)
			{
				g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_WinningKill);
			}
			if ( killedEntity == localClient && localClient != 0)
			{
				g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_VictimOnFinalKillcam);
			}
		}

		if(playerId == 0)  // overall the game was a Draw (at the top of the scoreboard)...
		{
			gameOverType = (clientScoreIsTop ? EGOT_Draw : EGOT_Lose);  // ...but whether or not the local client considers the game a Draw or a Loss depends on whether they are one of the players who are drawn at the top
		}
		else
		{
			gameOverType = (localPlayerWon ? EGOT_Win : EGOT_Lose);
		}
		CRY_ASSERT(gameOverType != EGOT_Unknown);

		EAnnouncementID announcementId = INVALID_ANNOUNCEMENT_ID;

		GetGameOverAnnouncement(gameOverType, clientScore, highestPlayerScore, announcementId);

		if (!m_pGameRules->GetRoundsModule())
		{
			// rounds will play the one shot themselves
			CAudioSignalPlayer::JustPlay("RoundEndOneShot");
		}
		SHUDEventWrapper::OnGameEnd((int) playerId, clientScoreIsTop, reason, NULL, ESVC_DrawResolution_invalid, announcementId);	
	}

	m_pGameRules->GameOver( gameOverType );
}
