// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYER_PROGRESSION_TYPES_H
#define __PLAYER_PROGRESSION_TYPES_H

#pragma once

#include "ProgressionUnlocks.h"

typedef std::vector<SUnlock> TUnlockElements;
typedef std::vector<TUnlockElements> TUnlockElementsVector;

enum EPPSuitData
{
	ePPS_Level = 0,
	ePPS_MaxLevel,
	ePPS_XP,
	ePPS_LifetimeXP,
	ePPS_XPToNextLevel,
	ePPS_XPLastMatch,
	ePPS_XPInCurrentLevel,
	ePPS_XPMatchStart,
	ePPS_MatchStartLevel,
	ePPS_MatchStartXPInCurrentLevel,
	ePPS_MatchStartXPToNextLevel,
};

#endif //#ifndef __PLAYER_PROGRESSION_TYPES_H