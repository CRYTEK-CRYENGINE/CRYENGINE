// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for Game Lobby events

	-------------------------------------------------------------------------
	History:
	- 06:09:2012  : Created by Jim Bamford

*************************************************************************/

#ifndef _IGAMELOBBYEVENTLISTENER_H_
#define _IGAMELOBBYEVENTLISTENER_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include <CryLobby/ICryLobbyUI.h>

class IGameLobbyEventListener
{
public:
	virtual ~IGameLobbyEventListener() {}

	virtual void InsertedUser(CryUserID userId, const char *userName) = 0;
	virtual void SessionChanged(const CrySessionHandle inOldSession, const CrySessionHandle inNewSession) = 0;
};

#endif // _IGAMELOBBYEVENTLISTENER_H_