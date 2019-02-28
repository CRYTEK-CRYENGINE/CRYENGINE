// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
	
	-------------------------------------------------------------------------
	History:
	- 09:09:2009  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GAME_RULES_ACTOR_ACTION_MODULE_H_
#define _GAME_RULES_ACTOR_ACTION_MODULE_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include <IGameRulesSystem.h>

struct IActor;

class IGameRulesActorActionModule
{
public:
	virtual ~IGameRulesActorActionModule() {}

	virtual void Init(XmlNodeRef xml) = 0;
	virtual void PostInit() = 0;

	virtual void OnActorAction(IActor *pActor, const ActionId& actionId, int activationMode, float value) = 0;
};

#endif // _GAME_RULES_ACTOR_ACTION_MODULE_H_