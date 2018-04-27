// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_STATS_H__
#define __CRYPSN2_STATS_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

		#include "CryStats.h"
		#include "CryPSN2Support.h"

		#if USE_CRY_STATS

			#define MAX_PSN_TUS_PROFILE_SIZE (3000)                 //-- 3000 bytes (not 3K!) to match the Xbox 360 limitation.
//-- We have to limit the buffer to a multiple of 16 bytes for rijndael to work, so this works out as 2992 bytes
//-- for the PSN data, not 3000
			#define MAX_PSN_PROFILE_SETTING_BUFFER_SIZE (MAX_PSN_TUS_PROFILE_SIZE & ~(15))

			#define MAX_PARALLEL_BOARD_WRITES           (6) //-- same as max number of leaderboards that can be written at the same time on xbox

class CCryPSNStats : public CCryStats
{
public:
	CCryPSNStats(CCryLobby* pLobby, CCryLobbyService* pService, CCryPSNSupport* pSupport);

	ECryLobbyError         Initialise();
	ECryLobbyError         Terminate();

	void                   Tick(CTimeValue tv);

	virtual ECryLobbyError StatsRegisterLeaderBoards(SCryStatsLeaderBoardWrite* pBoards, uint32 numBoards, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg);
	virtual ECryLobbyError StatsWriteLeaderBoards(CrySessionHandle session, uint32 user, SCryStatsLeaderBoardWrite* pBoards, uint32 numBoards, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg);
	virtual ECryLobbyError StatsWriteLeaderBoards(CrySessionHandle session, CryUserID* pUserIDs, SCryStatsLeaderBoardWrite** ppBoards, uint32* pNumBoards, uint32 numUsers, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg);
	virtual ECryLobbyError StatsReadLeaderBoardByRankForRange(CryStatsLeaderBoardID board, uint32 startRank, uint32 num, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg);
	virtual ECryLobbyError StatsReadLeaderBoardByRankForUser(CryStatsLeaderBoardID board, uint32 user, uint32 num, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg);
	virtual ECryLobbyError StatsReadLeaderBoardByUserID(CryStatsLeaderBoardID board, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg);

	virtual ECryLobbyError StatsRegisterUserData(SCryLobbyUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg);
	virtual ECryLobbyError StatsWriteUserData(uint32 user, SCryLobbyUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg);
	virtual ECryLobbyError StatsReadUserData(uint32 user, CryLobbyTaskID* pTaskID, CryStatsReadUserDataCallback cb, void* pCbArg);
	virtual ECryLobbyError StatsReadUserData(uint32 user, CryUserID userID, CryLobbyTaskID* pTaskID, CryStatsReadUserDataCallback cb, void* pCbArg);

	static void            SupportCallback(ECryPSNSupportCallbackEvent ecb, SCryPSNSupportCallbackEventData& data, void* pArg);

private:
	enum ETask
	{
		eT_StatsRegisterLeaderBoards,

		eT_StatsWriteLeaderBoards,
		eST_StatsWriteLeaderBoardsAsyncPolling,

		eT_StatsReadLeaderBoardByRankForRange,
		eT_StatsReadLeaderBoardByRankForUser,
		eT_StatsReadLeaderBoardByUserID,
		eST_StatsReadLeaderBoardByRankForUserAsyncPolling,
		eST_StatsReadLeaderBoardAsyncPolling,

		eT_StatsRegisterUserData,

		eT_StatsWriteUserData,
		eST_StatsWriteUserDataAsyncPolling,

		eT_StatsReadUserData,
		eST_StatsReadUserDataAsyncPolling,

		eT_StatsVerifyString,
		eST_StatsVerifyStringAsyncPolling,
	};

	struct SRegisterLeaderBoardData
	{
		CryStatsLeaderBoardID boardID;
		CryLobbyUserDataID    scoreID;
		uint32                numColumns;

		struct SColumn
		{
			CryLobbyUserDataID dataID;
			uint16             columnID;
			uint16             dataType;
		};

		SColumn* pColumns;
	};

	struct SRegisterUserData
	{
		uint16 id;
		uint16 type;
	};

	struct STask : public CCryStats::STask
	{
		SceNpId npId;
		int     transaction;
	} m_task[MAX_STATS_TASKS];

	struct SCryPSNStatsBoardWriteInfo
	{
		SceNpScoreBoardId    boardId;
		SceNpScoreValue      score;
		SceNpScoreComment    comment;
		SceNpScoreGameInfo   data;
		SceNpScoreRankNumber tempRank;
		uint32               nCumulativeDataSize;
		int                  transaction;
	};

	struct SCryPSNStatsBoardReadInfo
	{
		int                  transaction;
		SceNpScoreBoardId    boardId;
		SceNpScoreRankNumber start;
		uint32               count;
		SceRtcTick           lastSortDate;
		SceNpScoreRankNumber totalSize;
	};

	struct SProfileDataBufferGroup
	{
		uint8 unencryptedBuffer[MAX_PSN_PROFILE_SETTING_BUFFER_SIZE];
		uint8 encryptedBuffer[MAX_PSN_TUS_PROFILE_SIZE];
	};

	ECryLobbyError StartTask(ETask etask, bool startRunning, uint32 user, CryStatsTaskID* pSTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	void           StartSubTask(ETask etask, CryStatsTaskID sTaskID);
	void           StartTaskRunning(CryStatsTaskID sTaskID);
	void           EndTask(CryStatsTaskID sTaskID);
	void           StopTaskRunning(CryStatsTaskID sTaskID);

	ECryLobbyError StatsWriteLeaderBoards(const SceNpId* const pMyNpId, SCryStatsLeaderBoardWrite** ppBoards, uint32* pNumBoards, uint32 numUsers, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg);
	void           TickStatsWriteLeaderBoards(CryStatsTaskID sTaskID);
	void           TickStatsWriteLeaderBoardsAsyncPolling(CryStatsTaskID sTaskID);

	void           StartStatsReadLeaderBoardByRankForRange(CryStatsTaskID sTaskID);
	void           TickStatsReadLeaderBoardByRankForRange(CryStatsTaskID sTaskID);
	void           StartStatsReadLeaderBoardByRankForUser(CryStatsTaskID sTaskID);
	void           TickStatsReadLeaderBoardByRankForUser(CryStatsTaskID sTaskID);
	void           TickStatsReadLeaderBoardByRankForUserPolling(CryStatsTaskID sTaskID);
	void           StartStatsReadLeaderBoardByUserID(CryStatsTaskID sTaskID);
	void           TickStatsReadLeaderBoardByUserID(CryStatsTaskID sTaskID);
	void           TickStatsReadLeaderBoardAsyncPolling(CryStatsTaskID sTaskID);

	ECryLobbyError FillColumnFromPSNLeaderboardData(SCryStatsLeaderBoardUserColumn*, SRegisterLeaderBoardData::SColumn*, uint8* pData, uint32* pOffset);
	void           EndStatsReadLeaderBoard(CryStatsTaskID sTaskID);
	void           EndStatsReadLeaderBoardByUserID(CryStatsTaskID sTaskID);

	void           TickStatsWriteUserData(CryStatsTaskID sTaskID);
	void           TickStatsWriteUserDataAsyncPolling(CryStatsTaskID sTaskID);

	void           StartStatsReadUserData(CryStatsTaskID sTaskID);
	void           TickStatsReadUserData(CryStatsTaskID sTaskID);
	void           TickStatsReadUserDataAsyncPolling(CryStatsTaskID sTaskID);
	void           EndStatsReadUserData(CryStatsTaskID sTaskID);

	void           TickStatsVerifyString(CryStatsTaskID sTaskID);
	void           TickStatsVerifyStringAsyncPolling(CryStatsTaskID sTaskID);

	void           ProcessErrorEvent(SCryPSNSupportCallbackEventData& data);

	ECryLobbyError CreateScoreTransactionHandle(int* pHandle);
	void           AbortScoreTransactionHandle(int* pHandle);
	void           FreeScoreTransactionHandle(int* pHandle);
	ECryLobbyError CreateTusTransactionHandle(int* pHandle);
	void           AbortTusTransactionHandle(int* pHandle);
	void           FreeTusTransactionHandle(int* pHandle);

	DynArray<SRegisterLeaderBoardData> m_leaderBoards;
	DynArray<SRegisterUserData>        m_userData;
	uint32                             m_numLeaderBoards;
	uint32                             m_numUserData;
	uint32                             m_nCumulativeUserDataSize;

	CCryPSNSupport*                    m_pPSNSupport;
};

		#endif // USE_CRY_STATS
	#endif   // USE_PSN
#endif     // CRY_PLATFORM_ORBIS

#endif // __CRYPSN2_STATS_H__
