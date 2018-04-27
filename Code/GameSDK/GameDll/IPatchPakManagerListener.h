// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
	-------------------------------------------------------------------------
	$Id$
	$DateTime$
	Description: 
		Interface for PatchPakManager events

	-------------------------------------------------------------------------
	History:
	- 25:10:2012  : Created by Jim Bamford

*************************************************************************/

#ifndef _IPATCHPAKMANAGERLISTENER_H_
#define _IPATCHPAKMANAGERLISTENER_H_ 

#if _MSC_VER > 1000
# pragma once
#endif

class IPatchPakManagerListener
{
public:
	virtual ~IPatchPakManagerListener() {}

	virtual void UpdatedPermissionsNowAvailable() = 0;
};

#endif // _IPATCHPAKMANAGERLISTENER_H_  

