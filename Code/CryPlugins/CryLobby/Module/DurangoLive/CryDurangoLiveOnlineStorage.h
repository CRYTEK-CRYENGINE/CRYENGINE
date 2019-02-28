// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: CCryDurangoLiveOnlineStorage implementation for Durango

   assuming:
   - TMS for user storage (global TMS, which we don't care)
   - CMS for global storage

   -------------------------------------------------------------------------
   History:
   - 26:06:2013 : Created by Yeonwoon JUNG

*************************************************************************/

#ifndef __CRYDURANGOLIVEONLINESTORAGE_H__
#define __CRYDURANGOLIVEONLINESTORAGE_H__

#pragma once

#include <CryNetwork/ICryOnlineStorage.h>

#include "CryLobby.h"

#if USE_DURANGOLIVE && USE_CRY_ONLINE_STORAGE

	#define MAXIMUM_PENDING_QUERIES (8)

enum ECryOnlineStorageStatus
{
	eCOSS_Idle,       //No queue item is being processed
	eCOSS_Working,    //A queue item is being processed
};

class CCryLobby;
class CCryLobbyService;
class CAsyncInfoPimpl;

class CCryDurangoLiveOnlineStorage : public CMultiThreadRefCount, public ICryOnlineStorage
{
public:
	CCryDurangoLiveOnlineStorage(CCryLobby* pLobby, CCryLobbyService* pService);

	virtual void   Tick(CTimeValue tv) throw();

	ECryLobbyError Initialise()   { return eCLE_Success; }
	ECryLobbyError Terminate()    { return eCLE_Success; }

	virtual bool   IsDead() const { return false; }

	//Add an operation to the queue
	virtual ECryLobbyError OnlineStorageDataQuery(CryOnlineStorageQueryData* pQueryData);

	void                   TriggerCallbackOnGameThread(const CryOnlineStorageQueryData& QueryData);

protected:
	CryOnlineStorageQueryData        m_pendingQueries[MAXIMUM_PENDING_QUERIES];

	uint8                            m_pendingCount;
	ECryOnlineStorageStatus          m_status;
	std::unique_ptr<CAsyncInfoPimpl> m_pAsyncOperation;
	CCryLobby*                       m_pLobby;
	CCryLobbyService*                m_pService;
};

#endif//USE_DURANGOLIVE && USE_CRY_ONLINE_STORAGE

#endif // __CRYDURANGOLIVEONLINESTORAGE_H__
