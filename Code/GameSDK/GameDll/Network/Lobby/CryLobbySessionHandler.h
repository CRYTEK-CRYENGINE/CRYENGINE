// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: CryLobby session handler implementation.

-------------------------------------------------------------------------
History:
- 08:12:2009 : Created By Ben Johnson

*************************************************************************/
#ifndef __CRYLOBBY_SESSION_HANDLER_H__
#define __CRYLOBBY_SESSION_HANDLER_H__

#include <IGameSessionHandler.h>
#include <CryLobby/ICryMatchMaking.h>

class CCryLobbySessionHandler : public IGameSessionHandler
{
public:
	CCryLobbySessionHandler();
	virtual ~CCryLobbySessionHandler();

	// IGameSessionHandler
	virtual bool ShouldCallMapCommand(const char *pLevelName, const char *pGameRules);
	virtual void JoinSessionFromConsole(CrySessionID session);
	virtual void LeaveSession();
	
	virtual int StartSession();
	virtual int EndSession();

	virtual void OnUserQuit();
	virtual void OnGameShutdown();

	virtual CrySessionHandle GetGameSessionHandle() const;
	virtual bool ShouldMigrateNub() const;

	virtual bool IsMultiplayer() const;
	virtual int GetNumberOfExpectedClients();

	virtual bool IsGameSessionMigrating() const;
	virtual bool IsMidGameLeaving() const;
	virtual bool IsGameSessionMigratable() const;
	// ~IGameSessionHandler

protected:
	bool m_userQuit;
};

#endif //__CRYLOBBY_SESSION_HANDLER_H__
