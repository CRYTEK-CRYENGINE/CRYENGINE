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

#ifndef _GAME_RULES_MP_ACTOR_ACTION_H_
#define _GAME_RULES_MP_ACTOR_ACTION_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "IGameRulesActorActionModule.h"

class CGameRules;

class CGameRulesMPActorAction : public IGameRulesActorActionModule
{
public:
	CGameRulesMPActorAction();
	virtual ~CGameRulesMPActorAction();

	virtual void Init(XmlNodeRef xml);
	virtual void PostInit();

	virtual void OnActorAction(IActor *pActor, const ActionId& actionId, int activationMode, float value);
};

#endif // _GAME_RULES_MP_ACTOR_ACTION_H_
