// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Game rules module for games that use teams but handle the victory
		messages using the player's individual scores
	-------------------------------------------------------------------------
	History:
	- 30:05:2012  : Created by Sergi Juarez

*************************************************************************/

#ifndef _GAME_RULES_OBJECTIVE_VICTORY_CONDITIONS_INDIVIDUAL_H_
#define _GAME_RULES_OBJECTIVE_VICTORY_CONDITIONS_INDIVIDUAL_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameRulesObjectiveVictoryConditionsTeam.h"

class CGameRules;

class CGameRulesObjectiveVictoryConditionsIndividualScore : public CGameRulesObjectiveVictoryConditionsTeam
{
public:
	virtual void	OnEndGame(int teamId, EGameOverReason reason, ESVC_DrawResolution winningResolution=ESVC_DrawResolution_invalid, EntityId killedEntity=0, EntityId shooterEntity=0);
};

#endif // _GAME_RULES_OBJECTIVE_VICTORY_CONDITIONS_TEAM_H_