// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: 
Interface for a class that received events when clients score
-------------------------------------------------------------------------
History:
- 12:02:2010  : Created by Ben Parbury

*************************************************************************/

#ifndef _IGAME_RULES_CLIENT_SCORE_LISTENER_H_
#define _IGAME_RULES_CLIENT_SCORE_LISTENER_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include "GameRulesTypes.h"

class IGameRulesClientScoreListener
{
public:
	virtual ~IGameRulesClientScoreListener() {}

	virtual void ClientScoreEvent(EGameRulesScoreType type, int points, EXPReason inReason, int currentTeamScore) = 0;
};

#endif // _IGAME_RULES_CLIENT_SCORE_LISTENER_H_
