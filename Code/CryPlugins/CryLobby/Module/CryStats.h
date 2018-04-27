// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYSTATS_H__
#define __CRYSTATS_H__

#pragma once

#include <CryLobby/ICryStats.h>
#include "CryLobby.h"
#include "CryMatchMaking.h"

#if USE_CRY_STATS

	#define MAX_STATS_PARAMS 7
	#define MAX_STATS_TASKS  5

typedef uint32 CryStatsTaskID;
const CryStatsTaskID CryStatsInvalidTaskID = 0xffffffff;

class CCryStats : public CMultiThreadRefCount, public ICryStats
{
public:
	CCryStats(CCryLobby* pLobby, CCryLobbyService* pService);

	ECryLobbyError                   Initialise();
	ECryLobbyError                   Terminate();

	virtual bool                     IsDead() const                                                                                                                                                       { return false; }

	virtual ECryLobbyError           StatsWriteUserData(CryUserID* pUserIDs, SCryLobbyUserData** ppData, uint32* pNumData, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg) { return eCLE_InvalidRequest; };
	virtual void                     CancelTask(CryLobbyTaskID lTaskID);
	virtual void                     SetLeaderboardType(ECryLobbyLeaderboardType leaderboardType)                                                                                                         {}
	virtual void                     RegisterLeaderboardNameIdPair(string name, uint32 id)                                                                                                                {}
	virtual ECryLobbyLeaderboardType GetLeaderboardType()                                                                                                                                                 { return eCLLT_P2P; }

protected:

	struct  STask
	{
		CryLobbyTaskID        lTaskID;
		ECryLobbyError        error;
		uint32                startedTask;
		uint32                subTask;
		CryLobbySessionHandle session;
		void*                 pCb;
		void*                 pCbArg;
		TMemHdl               paramsMem[MAX_STATS_PARAMS];
		uint32                paramsNum[MAX_STATS_PARAMS];
		bool                  used;
		bool                  running;
		bool                  canceled;
	};

	ECryLobbyError StartTask(uint32 eTask, bool startRunning, CryStatsTaskID* pSTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	virtual void   FreeTask(CryStatsTaskID sTaskID);
	ECryLobbyError CreateTaskParamMem(CryStatsTaskID sTaskID, uint32 param, const void* pParamData, size_t paramDataSize);
	void           UpdateTaskError(CryStatsTaskID sTaskID, ECryLobbyError error);

	ECryLobbyError EncryptUserData(uint32 nBufferSize, const uint8* pUnencrypted, uint8* pEncrypted);
	ECryLobbyError DecryptUserData(uint32 nBufferSize, const uint8* pEncrypted, uint8* pDecrypted);

	STask*            m_pTask[MAX_STATS_TASKS];
	CCryLobby*        m_pLobby;
	CCryLobbyService* m_pService;
};

#endif // USE_CRY_STATS
#endif // __CRYSTATS_H__
