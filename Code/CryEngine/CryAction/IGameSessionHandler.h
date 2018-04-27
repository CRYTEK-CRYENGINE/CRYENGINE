// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Interface to handle sessions.

   -------------------------------------------------------------------------
   History:
   - 08:12:2009 : Created By Ben Johnson

*************************************************************************/

#ifndef __I_GAME_SESSION_HANDLER_H__
#define __I_GAME_SESSION_HANDLER_H__

#include <CryLobby/CommonICryLobby.h>

struct SGameStartParams;

struct IGameSessionHandler
{
public:
	virtual ~IGameSessionHandler() {}

	virtual bool             ShouldCallMapCommand(const char* pLevelName, const char* pGameRules) = 0;
	virtual void             JoinSessionFromConsole(CrySessionID session) = 0;
	virtual int              EndSession() = 0;

	virtual int              StartSession() = 0;
	virtual void             LeaveSession() = 0;

	virtual void             OnUserQuit() = 0;
	virtual void             OnGameShutdown() = 0;

	virtual CrySessionHandle GetGameSessionHandle() const = 0;
	virtual bool             ShouldMigrateNub() const = 0;

	virtual bool             IsMultiplayer() const = 0;

	virtual int              GetNumberOfExpectedClients() = 0;

	virtual bool             IsGameSessionMigrating() const = 0;
	virtual bool             IsMidGameLeaving() const = 0;
};

#endif //__I_GAME_SESSION_HANDLER_H__
