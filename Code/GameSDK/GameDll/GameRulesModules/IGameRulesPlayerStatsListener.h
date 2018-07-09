// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for a class that receives events when playerstats change
	-------------------------------------------------------------------------
	History:
	- 18:11:2009  : Created by Thomas Houghton

*************************************************************************/

#ifndef _IGAME_RULES_PLAYERSTATS_LISTENER_H_
#define _IGAME_RULES_PLAYERSTATS_LISTENER_H_

#if _MSC_VER > 1000
# pragma once
#endif

struct SGameRulesPlayerStat;

class IGameRulesPlayerStatsListener
{
public:
	virtual ~IGameRulesPlayerStatsListener() {}

	virtual void ClPlayerStatsNetSerializeReadDeath(const SGameRulesPlayerStat* s, uint16 prevDeathsThisRound, uint8 prevFlags) = 0;

};

#endif // _IGAME_RULES_PLAYERSTATS_LISTENER_H_
