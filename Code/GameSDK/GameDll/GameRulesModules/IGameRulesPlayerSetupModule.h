// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 

	-------------------------------------------------------------------------
	History:
	- 07:09:2009  : Created by Colin Gulliver

*************************************************************************/

#ifndef _GameRulesPlayerSetupModule_h_
#define _GameRulesPlayerSetupModule_h_

#if _MSC_VER > 1000
# pragma once
#endif


class IGameRulesPlayerSetupModule
{
public:
	virtual ~IGameRulesPlayerSetupModule() {}

	virtual void Init(XmlNodeRef xml) = 0;
	virtual void PostInit() = 0;

	virtual void OnClientConnect(int channelId, bool isReset, const char* playerName, bool isSpectator) = 0;
	virtual void OnPlayerRevived(EntityId playerId) = 0;
	virtual void OnActorJoinedFromSpectate(IActor* pActor, int channelId) = 0;

	virtual void SvOnStartNewRound(bool isReset) = 0;
};

#endif // _GameRulesPlayerSetupModule_h_