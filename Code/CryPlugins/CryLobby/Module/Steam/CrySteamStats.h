// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamStats.h
//  Created:     11/10/2012 by Andrew Catlender
//  Modified:    05/04/2014 by Michiel Meesters
//  Description: Integration of Steamworks API into CryStats
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

#ifndef __CRYSTEAMSTATS_H__
#define __CRYSTEAMSTATS_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "CryStats.h"

#if USE_STEAM && USE_CRY_STATS

	#include "steam/steam_api.h"
	#include "CrySteamMatchMaking.h"

////////////////////////////////////////////////////////////////////////////////

class CCrySteamStats : public CCryStats
{
public:
	CCrySteamStats(CCryLobby* pLobby, CCryLobbyService* pService);

	ECryLobbyError         Initialise();
	void                   Tick(CTimeValue tv);
	void                   EnterInteractiveGameSession();
	void                   LeaveInteractiveGameSession();

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

	virtual void           RegisterLeaderboardNameIdPair(string name, uint32 id);

private:
	enum ETask
	{
		eT_StatsRegisterLeaderBoards,
		eT_StatsWriteLeaderBoards,
		eT_StatsReadLeaderBoardByRankForRange,
		eT_StatsReadLeaderBoardByRankForUser,
		eT_StatsReadLeaderBoardByUserID,
		eT_StatsRegisterUserData,
		eT_StatsWriteUserData,
		eT_StatsReadUserData
	};

	struct SRegisterLeaderBoardData
	{
		CryStatsLeaderBoardID boardID;
		CryLobbyUserDataID    scoreID;
		uint32                numColumns;

		struct SColumn
		{
			CryLobbyUserDataID dataID;
			CryLobbyUserDataID columnID;
			uint16             dataType;
		};

		SColumn* pColumns;
	};

	struct SRegisterUserData
	{
		CryLobbyUserDataID id;
		uint16             type;
	};

	struct  STask : public CCryStats::STask
	{
	};

	ECryLobbyError StartTask(ETask etask, bool startRunning, uint32 user, CryStatsTaskID* pSTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg);
	void           StartSubTask(ETask etask, CryStatsTaskID sTaskID);
	void           StartTaskRunning(CryStatsTaskID sTaskID);
	void           EndTask(CryStatsTaskID sTaskID);
	void           StopTaskRunning(CryStatsTaskID sTaskID);
	ECryLobbyError TickTask(CryStatsTaskID sTaskID);

	void           StartEnumerate(CryStatsTaskID sTaskID, uint32 bufferSize);
	ECryLobbyError GetLeaderBoardSpec(CryStatsLeaderBoardID board /*, XUSER_STATS_SPEC* pSpec*/);

	ECryLobbyError StatsWriteLeaderBoards(CrySessionHandle session /*, XUID* pXUIDs*/, SCryStatsLeaderBoardWrite** ppBoards, uint32* pNumBoards, uint32 numUsers, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg);
	void           StartStatsWriteLeaderBoards(CryStatsTaskID sTaskID);
	void           TickStatsWriteLeaderBoards(CryStatsTaskID sTaskID);

	void           StartStatsReadLeaderBoardByRankForRange(CryStatsTaskID sTaskID);
	void           TickStatsReadLeaderBoard(CryStatsTaskID sTaskID);
	void           EndStatsReadLeaderBoard(CryStatsTaskID sTaskID);
	void           StartStatsReadLeaderBoardByRankForUser(CryStatsTaskID sTaskID);

	void           StartStatsReadLeaderBoardByUserID(CryStatsTaskID sTaskID);

	void           StartStatsWriteUserData(CryStatsTaskID sTaskID);
	void           TickStatsWriteUserData(CryStatsTaskID sTaskID);

	void           StartStatsReadUserData(CryStatsTaskID sTaskID);
	void           TickStatsReadUserData(CryStatsTaskID sTaskID);
	void           EndStatsReadUserData(CryStatsTaskID sTaskID);

	CRYSTEAM_CALLRESULT(CCrySteamStats, OnFindLeaderboard, LeaderboardFindResult_t, m_callResultFindLeaderboard);
	CRYSTEAM_CALLRESULT(CCrySteamStats, OnDownloadedLeaderboard, LeaderboardScoresDownloaded_t, m_callResultDownloadLeaderboard);
	CRYSTEAM_CALLRESULT(CCrySteamStats, OnUploadedScore, LeaderboardScoreUploaded_t, m_callResultWriteLeaderboard);

	CRYSTEAM_CALLBACK(CCrySteamStats, OnUserStatsReceived, UserStatsReceived_t, m_callUserStatsReceived);
	CRYSTEAM_CALLBACK(CCrySteamStats, OnUserStatsStored, UserStatsStored_t, m_callUserStatsStored);

	STask                              m_task[MAX_STATS_TASKS];
	DynArray<SRegisterLeaderBoardData> m_leaderBoards;
	DynArray<SRegisterUserData>        m_userData;
	uint32                             m_numLeaderBoards;
	uint32                             m_numUserData;
	uint32                             m_nCumulativeUserDataSize;

	typedef std::map<uint32, string> TIdToNameMap;
	TIdToNameMap m_IdToNameMap;
};

////////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM && USE_CRY_STATS
#endif // __CRYSTEAMSTATS_H__
