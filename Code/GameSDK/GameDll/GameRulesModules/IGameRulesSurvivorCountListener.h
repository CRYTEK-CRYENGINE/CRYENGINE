// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for a class that receives events about the number of players
		remaining in lives-limited game modes
	-------------------------------------------------------------------------
	History:
	- 06:11:2009  : Created by Thomas Houghton

*************************************************************************/

#ifndef _IGAME_RULES_SURVIVOR_COUNT_LISTENER_H_
#define _IGAME_RULES_SURVIVOR_COUNT_LISTENER_H_

#if _MSC_VER > 1000
# pragma once
#endif

class IGameRulesSurvivorCountListener
{
public:
	virtual ~IGameRulesSurvivorCountListener() {}

	virtual void SvSurvivorCountRefresh(int count, const EntityId survivors[], int numKills) = 0;
};

#endif // _IGAME_RULES_SURVIVOR_COUNT_LISTENER_H_
