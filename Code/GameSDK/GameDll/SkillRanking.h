// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:	Class for calculating player skill rankings

-------------------------------------------------------------------------
History:
- 22:09:2010 : Created by Colin Gulliver

*************************************************************************/

#ifndef __SKILL_RANKING_H__
#define __SKILL_RANKING_H__

#if _MSC_VER > 1000
# pragma once
#endif

#include <CryCore/Containers/CryFixedArray.h>

class CSkillRanking
{
public:
	CSkillRanking();

	void AddPlayer(EntityId id, uint16 skillPoints, int playerPoints, int teamId, float fractionTimeInGame);

	void TeamGameFinished(int team1Score, int team2Score);
	void NonTeamGameFinished();

	bool GetSkillPoints(EntityId id, uint16 &result);

	void NextGame();
private:

	struct SSkillPlayerData
	{
		SSkillPlayerData() : m_skillPoints(0), m_newSkillScore(0), m_playerPoints(0), m_teamId(0) {}

		void Set(EntityId id, uint16 skillPoints, int playerPoints, int teamId, float fractionTimeInGame)
		{
			m_id = id;
			m_skillPoints = skillPoints;
			m_newSkillScore = m_skillPoints;
			m_playerPoints = (playerPoints > 0 ? playerPoints : 0);		// Make sure points is >= 0
			m_teamId = teamId;

			// Scale points depending on time in game in an attempt  to simulate what the score 
			// would have been had the person been in for the whole game
			if (fractionTimeInGame > 0.f)
			{
				m_playerPoints = (int) ( ((float)m_playerPoints) / fractionTimeInGame );
			}
		}

		EntityId m_id;
		uint16 m_skillPoints;
		uint16 m_newSkillScore;
		int m_playerPoints;
		int m_teamId;
	};

	float GetPlayerFactor(int playerIndex, float averagePlayerPoints, float averageSkillRank);
	uint16 GetNewSkillRank(uint16 currentSkillRank, float totalFactor);

	static const int MAX_PLAYERS = MAX_PLAYER_LIMIT;

	SSkillPlayerData m_players[MAX_PLAYERS];
	uint32 m_numPlayers;
};

#endif // __SKILL_RANKING_H__
