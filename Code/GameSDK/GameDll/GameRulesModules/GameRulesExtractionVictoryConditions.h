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

#ifndef _GAME_RULES_EXTRACTION_VICTORY_CONDITIONS_H_
#define _GAME_RULES_EXTRACTION_VICTORY_CONDITIONS_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameRulesStandardVictoryConditionsTeam.h"

class CGameRules;

class CGameRulesExtractionVictoryConditions : public CGameRulesStandardVictoryConditionsTeam
{
public:

	// IGameRulesVictoryConditionsModule
	virtual void	Update(float frameTime);
	// ~IGameRulesVictoryConditionsModule

	virtual void  TeamCompletedAnObjective(int teamId);

protected:

	void					UpdateResolutionData(int primaryTeam);
	void					CheckObjectives();
	virtual void	TimeLimitExpired();
};

#endif // _GAME_RULES_EXTRACTION_VICTORY_CONDITIONS_H_