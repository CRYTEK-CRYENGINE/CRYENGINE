// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for a class that received events when entities are killed
	-------------------------------------------------------------------------
	History:
	- 23:09:2009  : Created by Colin Gulliver

*************************************************************************/

#ifndef _IGAME_RULES_KILL_LISTENER_H_
#define _IGAME_RULES_KILL_LISTENER_H_

#if _MSC_VER > 1000
# pragma once
#endif

struct HitInfo;

class IGameRulesKillListener
{
public:
	virtual ~IGameRulesKillListener() {}

	virtual void OnEntityKilledEarly(const HitInfo &hitInfo) = 0;
	virtual void OnEntityKilled(const HitInfo &hitInfo) = 0;
};

#endif // _IGAME_RULES_KILL_LISTENER_H_
