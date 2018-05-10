// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Game rules module to handle victory conditions for team games that
		use are won by completing an objective
	-------------------------------------------------------------------------
	History:
	- 24:09:2009  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GAME_RULES_OBJECTIVE_VICTORY_CONDITIONS_TEAM_H_
#define _GAME_RULES_OBJECTIVE_VICTORY_CONDITIONS_TEAM_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameRulesStandardVictoryConditionsTeam.h"

class CGameRules;

class CGameRulesObjectiveVictoryConditionsTeam : public CGameRulesStandardVictoryConditionsTeam
{
public:

	// IGameRulesVictoryConditionsModule
	virtual void	Update(float frameTime);
	// ~IGameRulesVictoryConditionsModule

protected:

	bool CheckObjectives();
};

#endif // _GAME_RULES_OBJECTIVE_VICTORY_CONDITIONS_TEAM_H_