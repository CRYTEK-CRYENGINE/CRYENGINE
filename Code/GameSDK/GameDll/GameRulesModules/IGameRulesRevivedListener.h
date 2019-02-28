// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for a class that receives events when entities revive
	-------------------------------------------------------------------------
	History:
	- 05:11:2009  : Created by Thomas Houghton

*************************************************************************/

#ifndef _IGAME_RULES_REVIVED_LISTENER_H_
#define _IGAME_RULES_REVIVED_LISTENER_H_

#if _MSC_VER > 1000
# pragma once
#endif

class IGameRulesRevivedListener
{
public:
	virtual ~IGameRulesRevivedListener() {}

	virtual void EntityRevived(EntityId entityId) = 0;
};

#endif // _IGAME_RULES_REVIVED_LISTENER_H_
