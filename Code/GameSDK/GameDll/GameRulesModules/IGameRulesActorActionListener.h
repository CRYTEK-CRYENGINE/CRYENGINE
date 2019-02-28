// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
	
	-------------------------------------------------------------------------
	History:
	- 19/05/2010  : Created by Tom Houghton

*************************************************************************/

#ifndef _GAME_RULES_ACTOR_ACTION_LISTENER_H_
#define _GAME_RULES_ACTOR_ACTION_LISTENER_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include <IGameRulesSystem.h>

class IGameRulesActorActionListener
{
public:
	virtual ~IGameRulesActorActionListener() {}

	virtual void OnAction(const ActionId& actionId, int activationMode, float value) = 0;
};


#endif // _GAME_RULES_ACTOR_ACTION_LISTENER_H_