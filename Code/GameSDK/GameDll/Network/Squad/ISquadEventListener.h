// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for squad manager events

	-------------------------------------------------------------------------
	History:
	- 14:12:2011  : Created by Ben Johnson

*************************************************************************/

#ifndef _ISQUADEVENTLISTENER_H_
#define _ISQUADEVENTLISTENER_H_

#if _MSC_VER > 1000
# pragma once
#endif

#include <CryLobby/ICryLobbyUI.h>

class ISquadEventListener
{
public:
	// Local squad events
	enum ESquadEventType
	{
		eSET_CreatedSquad,
		eSET_MigratedSquad,
		eSET_JoinedSquad,
		eSET_LeftSquad,
	};

public:
	virtual ~ISquadEventListener() {}

	virtual void AddedSquaddie(CryUserID userId) = 0;
	virtual void RemovedSquaddie(CryUserID userId) = 0;
	virtual void UpdatedSquaddie(CryUserID userId) = 0;
	virtual void SquadLeaderChanged(CryUserID userId) = 0;

	virtual void SquadEvent(ISquadEventListener::ESquadEventType eventType) = 0;
};

#endif // _ISQUADEVENTLISTENER_H_
