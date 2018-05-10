// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
// -------------------------------------------------------------------------
//  File name:   CrySteamStats.cpp
//  Created:     11/10/2012 by Andrew Catlender
//  Modified:    05/04/2014 by Michiel Meesters
//  Description: Integration of Steamworks API into CryStats
// -------------------------------------------------------------------------
//
////////////////////////////////////////////////////////////////////////////

//	Note: Currently can only run 1 task at a time (will cancel previous tasks that are referenced in steam callbacks it requires)

#include "StdAfx.h"

#include "Steam/CrySteamStats.h"
#include "Steam/CrySteamMatchMaking.h"

////////////////////////////////////////////////////////////////////////////

#if USE_STEAM && USE_CRY_STATS

	#define STATS_PARAM_READ_LEADERBOARD_NUM          0 // num
	#define STATS_PARAM_READ_LEADERBOARD_PTR          1 // ptr
	#define STATS_PARAM_READ_LEADERBOARD_BOARD        1 // num
	#define STATS_PARAM_READ_LEADERBOARD_RESULTS      2 // ptr
	#define STATS_PARAM_READ_LEADERBOARD_START        2 // num
	#define STATS_PARAM_READ_LEADERBOARD_GAME_ROWS    3 // ptr
	#define STATS_PARAM_READ_LEADERBOARD_GAME_COLUMNS 4 // ptr
	#define STATS_PARAM_READ_USERDATA_NUM             5 // num
	#define STATS_PARAM_READ_USERDATA_PTR             5 // ptr
	#define STATS_PARAM_WRITE_USERDATA_PTR            6 // ptr

	#define MAX_STEAM_COLUMNS                         64

//-- We have to limit the buffer to a multiple of 16 bytes for rijndael to work, so this works out as 992 bytes for each
//- of the 3 profile settings, not 1000 bytes per setting.
	#define MAX_STEAM_PROFILE_SETTING_BUFFER_SIZE (1000 & ~(15))

////////////////////////////////////////////////////////////////////////////

CCrySteamStats::CCrySteamStats(CCryLobby* pLobby, CCryLobbyService* pService)
	: CCryStats(pLobby, pService)
	, m_numLeaderBoards(0)
	, m_numUserData(0)
	, m_callUserStatsReceived(this, &CCrySteamStats::OnUserStatsReceived)
	, m_callUserStatsStored(this, &CCrySteamStats::OnUserStatsStored)
{
	// Make the CCryStats base pointers point to our data so we can use the common code in CCryStats
	for (uint32 i = 0; i < MAX_STATS_TASKS; i++)
	{
		CCryStats::m_pTask[i] = &m_task[i];
	}
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::Initialise()
{
	ECryLobbyError error = CCryStats::Initialise();

	return error;
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::EnterInteractiveGameSession()
{
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::LeaveInteractiveGameSession()
{
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::Tick(CTimeValue tv)
{
	for (uint32 i = 0; i < MAX_STATS_TASKS; i++)
	{
		STask* pTask = &m_task[i];

	#if ENABLE_CRYLOBBY_DEBUG_TESTS
		if (pTask->used)
		{
			if (m_pLobby->DebugTickCallStartTaskRunning(pTask->lTaskID))
			{
				StartTaskRunning(i);
				continue;
			}
			if (!m_pLobby->DebugOKToTickTask(pTask->lTaskID, pTask->running))
			{
				continue;
			}
		}
	#endif

		if (pTask->used && pTask->running)
		{
			switch (pTask->subTask)
			{
			case eT_StatsWriteLeaderBoards:
				TickStatsWriteLeaderBoards(i);
				break;

			case eT_StatsReadLeaderBoardByRankForRange:
			case eT_StatsReadLeaderBoardByRankForUser:
			case eT_StatsReadLeaderBoardByUserID:
				TickStatsReadLeaderBoard(i);
				break;

			case eT_StatsWriteUserData:
				TickStatsWriteUserData(i);
				break;

			case eT_StatsReadUserData:
				TickStatsReadUserData(i);
				break;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StartTask(ETask etask, bool startRunning, uint32 user, CryStatsTaskID* pSTaskID, CryLobbyTaskID* pLTaskID, CryLobbySessionHandle h, void* pCb, void* pCbArg)
{
	CryStatsTaskID tmpSTaskID;
	CryStatsTaskID* pUseSTaskID = pSTaskID ? pSTaskID : &tmpSTaskID;
	ECryLobbyError error = CCryStats::StartTask(etask, startRunning, pUseSTaskID, pLTaskID, h, pCb, pCbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[*pUseSTaskID];
	}
	else if (error != eCLE_TooManyTasks)
	{
		FreeTask(*pUseSTaskID);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::StartSubTask(ETask etask, CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];

	pTask->subTask = etask;
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::StartTaskRunning(CryStatsTaskID sTaskID)
{
	LOBBY_AUTO_LOCK;
	STask* pTask = &m_task[sTaskID];

	if (pTask->used)
	{
	#if ENABLE_CRYLOBBY_DEBUG_TESTS
		ECryLobbyError error;

		if (!m_pLobby->DebugOKToStartTaskRunning(pTask->lTaskID))
		{
			return;
		}

		if (m_pLobby->DebugGenerateError(pTask->lTaskID, error))
		{
			UpdateTaskError(sTaskID, error);
			StopTaskRunning(sTaskID);
			return;
		}
	#endif

		pTask->running = true;

		switch (pTask->startedTask)
		{
		case eT_StatsRegisterLeaderBoards:
		case eT_StatsRegisterUserData:
			StopTaskRunning(sTaskID);
			break;

		case eT_StatsWriteLeaderBoards:
			StartStatsWriteLeaderBoards(sTaskID);
			break;

		case eT_StatsReadLeaderBoardByRankForRange:
			StartStatsReadLeaderBoardByRankForRange(sTaskID);
			break;

		case eT_StatsReadLeaderBoardByRankForUser:
			StartStatsReadLeaderBoardByRankForUser(sTaskID);
			break;

		case eT_StatsReadLeaderBoardByUserID:
			StartStatsReadLeaderBoardByUserID(sTaskID);
			break;

		case eT_StatsWriteUserData:
			StartStatsWriteUserData(sTaskID);
			break;

		case eT_StatsReadUserData:
			StartStatsReadUserData(sTaskID);
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::EndTask(CryStatsTaskID sTaskID)
{
	LOBBY_AUTO_LOCK;

	STask* pTask = &m_task[sTaskID];

	if (pTask->used)
	{
		if (pTask->pCb)
		{
			switch (pTask->startedTask)
			{
			case eT_StatsRegisterLeaderBoards:
			case eT_StatsWriteLeaderBoards:
			case eT_StatsRegisterUserData:
			case eT_StatsWriteUserData:
				((CryStatsCallback)pTask->pCb)(pTask->lTaskID, pTask->error, pTask->pCbArg);
				break;

			case eT_StatsReadLeaderBoardByRankForRange:
			case eT_StatsReadLeaderBoardByRankForUser:
			case eT_StatsReadLeaderBoardByUserID:
				EndStatsReadLeaderBoard(sTaskID);
				break;

			case eT_StatsReadUserData:
				EndStatsReadUserData(sTaskID);
				break;
			}
		}

		NetLog("[Steam] Stats EndTask %d (%d) Result %d", pTask->startedTask, pTask->subTask, pTask->error);

		FreeTask(sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::StopTaskRunning(CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];

	if (pTask->used)
	{
		pTask->running = false;

		TO_GAME_FROM_LOBBY(&CCrySteamStats::EndTask, this, sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::TickTask(CryStatsTaskID sTaskID)
{
	return eCLE_Pending;
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsRegisterLeaderBoards(SCryStatsLeaderBoardWrite* pBoards, uint32 numBoards, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg)
{
	ECryLobbyError error = eCLE_Success;

	LOBBY_AUTO_LOCK;

	CryStatsTaskID sTaskID;

	error = StartTask(eT_StatsRegisterLeaderBoards, false, 0, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

	if (error == eCLE_Success)
	{
		if ((m_numLeaderBoards + numBoards) > (uint32)m_leaderBoards.size())
		{
			m_leaderBoards.resize(m_leaderBoards.size() + numBoards);
		}

		for (uint32 i = 0; i < numBoards; i++)
		{
			SRegisterLeaderBoardData* pRegisterData = &m_leaderBoards[m_numLeaderBoards];
			SCryStatsLeaderBoardWrite* pBoardData = &pBoards[i];

			pRegisterData->boardID = pBoardData->id;
			pRegisterData->scoreID = pBoardData->data.score.id;
			pRegisterData->numColumns = pBoardData->data.numColumns;

			if (pRegisterData->numColumns > 0)
			{
				pRegisterData->pColumns = new SRegisterLeaderBoardData::SColumn[pRegisterData->numColumns];

				for (uint32 j = 0; j < pRegisterData->numColumns; j++)
				{
					SRegisterLeaderBoardData::SColumn* pRegisterColumn = &pRegisterData->pColumns[j];
					SCryStatsLeaderBoardUserColumn* pBoardColumn = &pBoardData->data.pColumns[j];

					pRegisterColumn->columnID = pBoardColumn->columnID;
					pRegisterColumn->dataID = pBoardColumn->data.m_id;
					pRegisterColumn->dataType = pBoardColumn->data.m_type;
				}
			}
			else
			{
				pRegisterData->pColumns = NULL;
			}

			m_numLeaderBoards++;
		}

		FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);
	}

	NetLog("[Steam] Start StatsRegisterLeaderBoards return %d", error);

	return error;
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsWriteLeaderBoards(CrySessionHandle session, uint32 user, SCryStatsLeaderBoardWrite* pBoards, uint32 numBoards, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	SCryStatsLeaderBoardWrite* pLeaderBoards[1];
	uint32 numLeaderBoards[1];

	pLeaderBoards[0] = pBoards;
	numLeaderBoards[0] = numBoards;
	ECryLobbyError error = StatsWriteLeaderBoards(session, pLeaderBoards, numLeaderBoards, 1, pTaskID, cb, pCbArg);

	if (error != eCLE_Success)
	{
		NetLog("[Steam]: StatsWriteLeaderBoards not started, error: %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsWriteLeaderBoards(CrySessionHandle session, CryUserID* pUserIDs, SCryStatsLeaderBoardWrite** ppBoards, uint32* pNumBoards, uint32 numUsers, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = StatsWriteLeaderBoards(session, ppBoards, pNumBoards, numUsers, pTaskID, cb, pCbArg);
	return error;
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsWriteLeaderBoards(CrySessionHandle session, SCryStatsLeaderBoardWrite** ppBoards, uint32* pNumBoards, uint32 numUsers, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg)
{
	ECryLobbyError error = eCLE_Success;

	CryStatsTaskID sTaskID;

	error = StartTask(eT_StatsWriteLeaderBoards, false, 0, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

	if (error != eCLE_TooManyTasks)
	{
		UpdateTaskError(sTaskID, CreateTaskParamMem(sTaskID, STATS_PARAM_READ_LEADERBOARD_PTR, NULL, sizeof(SCryStatsLeaderBoardWrite)));

		STask* pTask = &m_task[sTaskID];
		SCryStatsLeaderBoardWrite* writeInfo = (SCryStatsLeaderBoardWrite*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_LEADERBOARD_PTR]);
		memcpy(writeInfo, ppBoards[0], sizeof(SCryStatsLeaderBoardWrite));

		UpdateTaskError(sTaskID, CreateTaskParamMem(sTaskID, STATS_PARAM_READ_LEADERBOARD_GAME_COLUMNS, NULL, writeInfo->data.numColumns * sizeof(SCryStatsLeaderBoardUserColumn)));
		SCryStatsLeaderBoardUserColumn* userColumns = (SCryStatsLeaderBoardUserColumn*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_LEADERBOARD_GAME_COLUMNS]);

		for (int i = 0; i < writeInfo->data.numColumns; i++)
		{
			new(&userColumns[i].data.m_id)SCryLobbyUserData();
			userColumns[i].data = writeInfo->data.pColumns[i].data;
		}

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);
		}
		else
		{
			FreeTask(sTaskID);
		}
	}

	NetLog("[Steam] Start StatsWriteLeaderBoards return %d", error);

	return error;
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsReadLeaderBoardByRankForRange(CryStatsLeaderBoardID board, uint32 startRank, uint32 num, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	CryStatsTaskID sTaskID;
	ECryLobbyError error = StartTask(eT_StatsReadLeaderBoardByRankForRange, false, 0, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[sTaskID];

		pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_BOARD] = board;
		pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_START] = startRank;
		pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_NUM] = num;

		FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);
	}

	NetLog("[Steam] Start StatsWriteLeaderBoards return %d", error);

	return error;
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::OnUploadedScore(LeaderboardScoreUploaded_t* pParam, bool ioFailure)
{
	if (m_callResultWriteLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = &m_task[m_callResultWriteLeaderboard.m_taskID];
		if (ioFailure)
		{
			UpdateTaskError(m_callResultWriteLeaderboard.m_taskID, eCLE_InternalError);
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[Steam]: Failed to upload score to the leaderboard");
		}

		StopTaskRunning(m_callResultWriteLeaderboard.m_taskID);
		m_callResultWriteLeaderboard.m_taskID = CryMatchMakingInvalidTaskID;
		m_callResultFindLeaderboard.m_taskID = CryMatchMakingInvalidTaskID;
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::OnDownloadedLeaderboard(LeaderboardScoresDownloaded_t* pParam, bool ioFailure)
{
	if (m_callResultDownloadLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
	{
		ISteamUserStats* pSteamUserStats = SteamUserStats();
		ISteamFriends* pSteamFriends = SteamFriends();
		if (!pSteamUserStats || !pSteamFriends)
		{
			UpdateTaskError(m_callResultDownloadLeaderboard.m_taskID, eCLE_SteamInitFailed);
			StopTaskRunning(m_callResultDownloadLeaderboard.m_taskID);
			return;
		};

		STask* pTask = &m_task[m_callResultDownloadLeaderboard.m_taskID];

		if (!ioFailure)
		{
			// Success: send back the results
			SCryStatsLeaderBoardReadResult result;
			result.numRows = pParam->m_cEntryCount;

			UpdateTaskError(m_callResultDownloadLeaderboard.m_taskID, CreateTaskParamMem(m_callResultDownloadLeaderboard.m_taskID, STATS_PARAM_READ_LEADERBOARD_GAME_ROWS, NULL, result.numRows * sizeof(SCryStatsLeaderBoardReadRow)));
			result.pRows = (SCryStatsLeaderBoardReadRow*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_LEADERBOARD_GAME_ROWS]);

			UpdateTaskError(m_callResultDownloadLeaderboard.m_taskID, CreateTaskParamMem(m_callResultDownloadLeaderboard.m_taskID, STATS_PARAM_READ_LEADERBOARD_GAME_COLUMNS, NULL, MAX_STEAM_COLUMNS * result.numRows * sizeof(SCryStatsLeaderBoardUserColumn)));
			SCryStatsLeaderBoardUserColumn* columns = (SCryStatsLeaderBoardUserColumn*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_LEADERBOARD_GAME_COLUMNS]);

			for (int i = 0; i < result.numRows; i++)
			{
				LeaderboardEntry_t leaderboardEntry;
				int32 details[MAX_STEAM_COLUMNS];
				pSteamUserStats->GetDownloadedLeaderboardEntry(pParam->m_hSteamLeaderboardEntries, i, &leaderboardEntry, details, MAX_STEAM_COLUMNS);

				result.pRows[i].data.numColumns = leaderboardEntry.m_cDetails;

				for (int j = 0; j < leaderboardEntry.m_cDetails; j++)
				{
					int colIdx = j + i * leaderboardEntry.m_cDetails;
					columns[colIdx].data.m_int32 = details[j];
					result.pRows[i].data.pColumns = &columns[colIdx];
				}

				const char* steamName = pSteamFriends->GetFriendPersonaName(leaderboardEntry.m_steamIDUser);
				cry_strcpy(result.pRows[i].name, steamName);
				result.pRows[i].data.score.score.m_int32 = leaderboardEntry.m_nScore;
				result.pRows[i].data.score.score.m_type = eCLUDT_Int32;
			}

			if (pTask->pCb)
			{
				((CryStatsReadLeaderBoardCallback)pTask->pCb)(pTask->lTaskID, pTask->error, &result, pTask->pCbArg);
			}

			StopTaskRunning(m_callResultDownloadLeaderboard.m_taskID);
			m_callResultDownloadLeaderboard.m_taskID = CryMatchMakingInvalidTaskID;
			m_callResultFindLeaderboard.m_taskID = CryMatchMakingInvalidTaskID;
		}
		else
		{
			UpdateTaskError(m_callResultDownloadLeaderboard.m_taskID, eCLE_InternalError);
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[Steam]: Failed to download the leaderboard");
		}
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::OnFindLeaderboard(LeaderboardFindResult_t* pParam, bool ioFailure)
{
	if (m_callResultFindLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
	{
		STask* pTask = &m_task[m_callResultFindLeaderboard.m_taskID];
		ISteamUserStats* pSteamUserStats = SteamUserStats();
		if (!pSteamUserStats)
		{
			UpdateTaskError(m_callResultFindLeaderboard.m_taskID, eCLE_SteamInitFailed);
		}
		else
		{
			if (!ioFailure && pParam->m_bLeaderboardFound)
			{
				// Success, manipulate or read the board here

				switch (pTask->startedTask)
				{

				case eT_StatsReadLeaderBoardByUserID:
				case eT_StatsReadLeaderBoardByRankForUser:
				{
					SteamAPICall_t handleToDownloadLB = pSteamUserStats->DownloadLeaderboardEntries(pParam->m_hSteamLeaderboard, k_ELeaderboardDataRequestGlobalAroundUser, 0, pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_NUM]);
					m_callResultDownloadLeaderboard.m_callResult.Set(handleToDownloadLB, this, &CCrySteamStats::OnDownloadedLeaderboard);
					m_callResultDownloadLeaderboard.m_taskID = m_callResultFindLeaderboard.m_taskID;
				}
				break;

				case eT_StatsReadLeaderBoardByRankForRange:
				{
					SteamAPICall_t handleToDownloadLB = pSteamUserStats->DownloadLeaderboardEntries(pParam->m_hSteamLeaderboard, k_ELeaderboardDataRequestGlobal, pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_START], pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_NUM]);
					m_callResultDownloadLeaderboard.m_callResult.Set(handleToDownloadLB, this, &CCrySteamStats::OnDownloadedLeaderboard);
					m_callResultDownloadLeaderboard.m_taskID = m_callResultFindLeaderboard.m_taskID;
				}
				break;

				case eT_StatsWriteLeaderBoards:
				{
					SCryStatsLeaderBoardWrite* writeInfo = (SCryStatsLeaderBoardWrite*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_LEADERBOARD_BOARD]);
					if (writeInfo->data.score.score.m_type != eCLUDT_Int32)
					{
						CRY_ASSERT_MESSAGE(false, "score has to be int32 format");
					}

					SCryStatsLeaderBoardUserColumn* userColumns = (SCryStatsLeaderBoardUserColumn*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_LEADERBOARD_GAME_COLUMNS]);
					int32* additionalInfo = writeInfo->data.numColumns > 0 ? (int32*)malloc(writeInfo->data.numColumns * sizeof(int32)) : NULL;
					for (int i = 0; i < writeInfo->data.numColumns; i++)
					{
						additionalInfo[i] = userColumns[i].data.m_int32;
					}

					SteamAPICall_t hUploadScore = pSteamUserStats->UploadLeaderboardScore(pParam->m_hSteamLeaderboard, k_ELeaderboardUploadScoreMethodForceUpdate, writeInfo->data.score.score.m_int32, additionalInfo, writeInfo->data.numColumns);
					m_callResultWriteLeaderboard.m_callResult.Set(hUploadScore, this, &CCrySteamStats::OnUploadedScore);
					m_callResultWriteLeaderboard.m_taskID = m_callResultFindLeaderboard.m_taskID;

					if (additionalInfo)
						free(additionalInfo);
				}
				break;

				}
			}
			else
			{
				UpdateTaskError(m_callResultFindLeaderboard.m_taskID, eCLE_InternalError);
			}
		}

		if (pTask->error != eCLE_Success)
		{
			NetLog("[Steam]: Failed to find the leaderboard");
		}
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::StartStatsReadLeaderBoardByRankForRange(CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];

	if (m_callResultFindLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
	{
		m_callResultFindLeaderboard.m_callResult.Cancel();

		// cancel subtask
		if (m_callResultDownloadLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
			m_callResultDownloadLeaderboard.m_callResult.Cancel();

		StopTaskRunning(m_callResultFindLeaderboard.m_taskID);
		NetLog("[Steam]: StartStatsReadLeaderBoardByRankForRange() - cancelled previous request");
	}

	ISteamUserStats* pSteamUserStats = SteamUserStats();
	if (pSteamUserStats)
	{
		TIdToNameMap::iterator it = m_IdToNameMap.find(pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_BOARD]);
		if (it != m_IdToNameMap.end())
		{
			SteamAPICall_t handleToLB = pSteamUserStats->FindLeaderboard(it->second);
			m_callResultFindLeaderboard.m_callResult.Set(handleToLB, this, &CCrySteamStats::OnFindLeaderboard);
			m_callResultFindLeaderboard.m_taskID = sTaskID;
		}
		else
		{
			UpdateTaskError(sTaskID, eCLE_LeaderBoardNotRegistered);
		}
	}
	else
	{
		UpdateTaskError(sTaskID, eCLE_SteamInitFailed);
	}

	NetLog("[Steam] StartStatsReadLeaderBoardByRankForRange result %d", pTask->error);

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::TickStatsReadLeaderBoard(CryStatsTaskID sTaskID)
{
	// unused - using steamcallback
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::EndStatsReadLeaderBoard(CryStatsTaskID sTaskID)
{
	// unused - using steamcallback
	// callback funcptr already called in OnDownloadedLeaderboard
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsReadLeaderBoardByRankForUser(CryStatsLeaderBoardID board, uint32 user, uint32 num, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	CryStatsTaskID sTaskID;
	ECryLobbyError error = StartTask(eT_StatsReadLeaderBoardByRankForUser, false, 0, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

	if (error == eCLE_Success)
	{
		STask* pTask = &m_task[sTaskID];

		pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_BOARD] = board;
		pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_START] = user;
		pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_NUM] = num;

		FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);
	}
	else
	{
		NetLog("[Steam]: ReadLeaderboard not started, error: %d", error);
	}

	return error;
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::StartStatsReadLeaderBoardByRankForUser(CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];

	if (m_callResultFindLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
	{
		m_callResultFindLeaderboard.m_callResult.Cancel();

		// cancel subtask
		if (m_callResultDownloadLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
			m_callResultDownloadLeaderboard.m_callResult.Cancel();

		StopTaskRunning(m_callResultFindLeaderboard.m_taskID);
		NetLog("[Steam]: StartStatsReadLeaderBoardByRankForUser() - cancelled previous request");
	}

	ISteamUserStats* pSteamUserStats = SteamUserStats();
	if (pSteamUserStats)
	{
		TIdToNameMap::iterator it = m_IdToNameMap.find(pTask->paramsNum[STATS_PARAM_READ_LEADERBOARD_BOARD]);
		if (it != m_IdToNameMap.end())
		{
			SteamAPICall_t handleToLB = pSteamUserStats->FindLeaderboard(it->second);
			m_callResultFindLeaderboard.m_callResult.Set(handleToLB, this, &CCrySteamStats::OnFindLeaderboard);
			m_callResultFindLeaderboard.m_taskID = sTaskID;
		}
		else
		{
			UpdateTaskError(sTaskID, eCLE_LeaderBoardNotRegistered);
		}
	}
	else
	{
		UpdateTaskError(sTaskID, eCLE_SteamInitFailed);
	}

	NetLog("[Steam] StartStatsReadLeaderBoardByRankForUser result %d", pTask->error);

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsReadLeaderBoardByUserID(CryStatsLeaderBoardID board, CryUserID* pUserIDs, uint32 numUserIDs, CryLobbyTaskID* pTaskID, CryStatsReadLeaderBoardCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	CryStatsTaskID sTaskID;
	ECryLobbyError error = StartTask(eT_StatsReadLeaderBoardByUserID, false, 0, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

	if (error == eCLE_Success)
	{
		// not implemented yet
		FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);

	}

	NetLog("[Steam] Start StatsReadLeaderBoardByUserID return %d", error);

	return error;
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::StartStatsReadLeaderBoardByUserID(CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];

	// not implemented yet

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::StartStatsWriteLeaderBoards(CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];

	if (m_callResultFindLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
	{
		m_callResultFindLeaderboard.m_callResult.Cancel();

		if (m_callResultWriteLeaderboard.m_taskID != CryMatchMakingInvalidTaskID)
			m_callResultWriteLeaderboard.m_callResult.Cancel();

		StopTaskRunning(m_callResultFindLeaderboard.m_taskID);
		NetLog("[Steam]: StartStatsWriteLeaderBoards() - cancelled previous request");
	}

	SCryStatsLeaderBoardWrite* writeInfo = (SCryStatsLeaderBoardWrite*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_LEADERBOARD_PTR]);

	ISteamUserStats* pSteamUserStats = SteamUserStats();
	if (pSteamUserStats)
	{
		TIdToNameMap::iterator it = m_IdToNameMap.find(writeInfo->id);
		if (it != m_IdToNameMap.end())
		{
			SteamAPICall_t handleToLB = pSteamUserStats->FindLeaderboard(it->second);
			m_callResultFindLeaderboard.m_callResult.Set(handleToLB, this, &CCrySteamStats::OnFindLeaderboard);
			m_callResultFindLeaderboard.m_taskID = sTaskID;
		}
		else
		{
			UpdateTaskError(sTaskID, eCLE_LeaderBoardNotRegistered);
		}
	}
	else
	{
		UpdateTaskError(sTaskID, eCLE_SteamInitFailed);
	}

	NetLog("[Steam] StartStatsWriteLeaderBoards result %d", pTask->error);

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(sTaskID);
		m_callResultFindLeaderboard.m_taskID = CryMatchMakingInvalidTaskID;
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::TickStatsWriteLeaderBoards(CryStatsTaskID sTaskID)
{
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsRegisterUserData(SCryLobbyUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	CryStatsTaskID sTaskID;
	ECryLobbyError error = StartTask(eT_StatsRegisterUserData, false, 0, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

	if (error == eCLE_Success)
	{
		if ((m_numUserData + numData) > (uint32)m_userData.size())
		{
			m_userData.resize(m_userData.size() + numData);
		}

		for (uint32 i = 0; (i < numData) && (error == eCLE_Success); i++)
		{
			uint32 itemSize;

			switch (pData[i].m_type)
			{
			case eCLUDT_Int64:
				itemSize = sizeof(pData[i].m_int64);
				break;

			case eCLUDT_Int32:
				itemSize = sizeof(pData[i].m_int32);
				break;

			case eCLUDT_Int16:
				itemSize = sizeof(pData[i].m_int16);
				break;

			case eCLUDT_Int8:
				itemSize = sizeof(pData[i].m_int8);
				break;

			case eCLUDT_Float64:
				itemSize = sizeof(pData[i].m_f64);
				break;

			case eCLUDT_Float32:
				itemSize = sizeof(pData[i].m_f32);
				break;

			case eCLUDT_Int64NoEndianSwap:
				itemSize = sizeof(pData[i].m_int64);
				break;

			default:
				itemSize = 0;
				CRY_ASSERT_MESSAGE(0, "CCrySteamStats::StatsRegisterUserData: Undefined data type");
				break;
			}

			if (error == eCLE_Success)
			{
				m_userData[m_numUserData].id = pData[i].m_id;
				m_userData[m_numUserData].type = pData[i].m_type;
				m_numUserData++;

				m_nCumulativeUserDataSize += itemSize;
			}
		}

		FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);
	}

	NetLog("[Steam] Start StatsRegisterUserData return %d", error);

	return error;
}

////////////////////////////////////////////////////////////////////////////

	#define STATS_PARAM_WRITE_USER_DATA_SETTINGS     0
	#define STATS_PARAM_WRITE_USER_DATA_BLOCKS_START 1
ECryLobbyError CCrySteamStats::StatsWriteUserData(uint32 user, SCryLobbyUserData* pData, uint32 numData, CryLobbyTaskID* pTaskID, CryStatsCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;

	if (m_numUserData > 0)
	{
		CryStatsTaskID sTaskID;

		error = StartTask(eT_StatsWriteUserData, false, user, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

		if (error != eCLE_TooManyTasks)
		{
			STask* pTask = &m_task[sTaskID];

			// store data in task
			UpdateTaskError(sTaskID, CreateTaskParamMem(sTaskID, STATS_PARAM_READ_USERDATA_PTR, NULL, numData * sizeof(SCryLobbyUserData)));
			SCryLobbyUserData* userData = (SCryLobbyUserData*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_USERDATA_PTR]);
			for (int i = 0; i < numData; i++)
			{
				new(&userData[i])SCryLobbyUserData();
				userData[i] = pData[i];
			}
			pTask->paramsNum[STATS_PARAM_READ_USERDATA_NUM] = numData;

			if (error == eCLE_Success)
			{
				FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);
			}
		}
	}
	else
	{
		error = eCLE_NoUserDataRegistered;
	}

	NetLog("[Steam] Stats StatsWriteUserData return %d", error);

	return error;
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::StartStatsWriteUserData(CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];

	ISteamUserStats* pSteamUserStats = SteamUserStats();

	if (pSteamUserStats)
	{
		NetLog("[Steam] Stats StartStatsWriteUserData result %d", pTask->error);

		SCryLobbyUserData* userData = (SCryLobbyUserData*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_READ_USERDATA_PTR]);
		int numData = pTask->paramsNum[STATS_PARAM_READ_USERDATA_NUM];

		for (int i = 0; i < numData; i++)
		{
			pSteamUserStats->SetStat(userData[i].m_id, userData[i].m_int32);
			NetLog("[Steam] Writing Stat: %s, val: %d", userData[i].m_id, userData[i].m_int32);
		}

		pSteamUserStats->StoreStats();
	}
	else
	{
		pTask->error = eCLE_SteamInitFailed;
	}

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::TickStatsWriteUserData(CryStatsTaskID sTaskID)
{
	ECryLobbyError error = TickTask(sTaskID);

	// not implemented yet

	if (error != eCLE_Pending)
	{
		UpdateTaskError(sTaskID, error);
		StopTaskRunning(sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsReadUserData(uint32 user, CryLobbyTaskID* pTaskID, CryStatsReadUserDataCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;

	if (m_numUserData > 0)
	{
		CryStatsTaskID sTaskID;

		if (error == eCLE_Success)
		{
			error = StartTask(eT_StatsReadUserData, false, user, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

			if (error == eCLE_Success)
			{
				FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);
			}
		}
	}
	else
	{
		error = eCLE_NoUserDataRegistered;
	}

	NetLog("[Steam] Stats StatsReadUserData return %d", error);

	return error;
}

////////////////////////////////////////////////////////////////////////////

ECryLobbyError CCrySteamStats::StatsReadUserData(uint32 user, CryUserID userID, CryLobbyTaskID* pTaskID, CryStatsReadUserDataCallback cb, void* pCbArg)
{
	LOBBY_AUTO_LOCK;

	ECryLobbyError error = eCLE_Success;

	if (m_numUserData > 0)
	{
		CryStatsTaskID sTaskID;

		error = StartTask(eT_StatsReadUserData, false, user, &sTaskID, pTaskID, CryLobbyInvalidSessionHandle, cb, pCbArg);

		if (error == eCLE_Success)
		{
			FROM_GAME_TO_LOBBY(&CCrySteamStats::StartTaskRunning, this, sTaskID);
		}
	}
	else
	{
		error = eCLE_NoUserDataRegistered;
	}

	NetLog("[Steam] Stats StatsReadUserData return %d", error);

	return error;
}

////////////////////////////////////////////////////////////////////////////

	#define STATS_PARAM_READ_USER_DATA_SETTINGIDS    0
	#define STATS_PARAM_READ_USER_DATA_RESULTS       1
	#define STATS_PARAM_READ_USER_DATA_GAME_RESULTS  2
	#define STATS_PARAM_READ_USER_DATA_DECRYPT_START 3
void CCrySteamStats::StartStatsReadUserData(CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];

	ISteamUserStats* pSteamUserStats = SteamUserStats();
	if (pSteamUserStats)
	{
		NetLog("[Steam] Stats StartStatsReadUserData result %d", pTask->error);

		pSteamUserStats->RequestCurrentStats();
		m_callUserStatsReceived.m_taskID = sTaskID;
	}
	else
	{
		pTask->error = eCLE_SteamInitFailed;
	}

	if (pTask->error != eCLE_Success)
	{
		StopTaskRunning(sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::TickStatsReadUserData(CryStatsTaskID sTaskID)
{
	ECryLobbyError error = TickTask(sTaskID);

	if (error != eCLE_Pending)
	{
		UpdateTaskError(sTaskID, error);
		StopTaskRunning(sTaskID);
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::RegisterLeaderboardNameIdPair(string name, uint32 id)
{
	m_IdToNameMap[id] = name;
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::OnUserStatsStored(UserStatsStored_t* pParam)
{
	NetLog("[Steam] CrySteamStats: Stats stored");
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::OnUserStatsReceived(UserStatsReceived_t* pParam)
{
	ISteamUtils* pSteamUtils = SteamUtils();
	ISteamUserStats* pSteamUserStats = SteamUserStats();
	if (!pSteamUserStats || !pSteamUtils)
	{
		if (m_callUserStatsReceived.m_taskID != CryLobbyInvalidTaskID)
		{
			UpdateTaskError(m_callUserStatsReceived.m_taskID, eCLE_SteamInitFailed);
			StopTaskRunning(m_callUserStatsReceived.m_taskID);
		}
		return;
	}
	if (pSteamUtils->GetAppID() == pParam->m_nGameID && m_callUserStatsReceived.m_taskID != CryLobbyInvalidTaskID)
	{
		if (k_EResultOK == pParam->m_eResult)
		{
			STask* pTask = &m_task[m_callUserStatsReceived.m_taskID];

			UpdateTaskError(m_callUserStatsReceived.m_taskID, CreateTaskParamMem(m_callUserStatsReceived.m_taskID, STATS_PARAM_WRITE_USERDATA_PTR, NULL, m_numUserData * sizeof(SCryLobbyUserData)));
			SCryLobbyUserData* pData = (SCryLobbyUserData*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_WRITE_USERDATA_PTR]);

			for (int i = 0; i < m_numUserData; i++)
			{
				new(&pData[i])SCryLobbyUserData();
			}

			if (pParam->m_eResult == k_EResultOK)
			{
				for (int i = 0; i < m_numUserData; i++)
				{
					pData[i].m_type = (ECryLobbyUserDataType)m_userData[i].type;

					switch (pData[i].m_type)
					{
					case eCLUDT_Int32:
						{
							int32 statVal = 0;
							if (!pSteamUserStats->GetStat(m_userData[i].id, &statVal))
							{
								NetLog("[Steam] Reading Stat failed: %s (setting to 0)", m_userData[i].id);
							}
							pData[i].m_id = m_userData[i].id;

							pData[i].m_int32 = statVal;
						}
						break;
					case eCLUDT_Float32:
						{
							float statVal = 0;
							if (!pSteamUserStats->GetStat(m_userData[i].id, &statVal))
							{
								NetLog("[Steam] Reading Stat failed: %s (setting to 0)", m_userData[i].id);
							}
							pData[i].m_id = m_userData[i].id;

							pData[i].m_f32 = statVal;
						}
						break;
					}
				}
				if (m_callUserStatsReceived.m_taskID != CryLobbyInvalidTaskID && pTask->pCb) // if someone asked for it through a task, give back the results
					StopTaskRunning(m_callUserStatsReceived.m_taskID);
			}
			else
			{
				UpdateTaskError(m_callUserStatsReceived.m_taskID, eCLE_InternalError);
			}

			// Stop task and reset callback
			if (m_callUserStatsReceived.m_taskID != CryLobbyInvalidTaskID)
				StopTaskRunning(m_callUserStatsReceived.m_taskID);

			m_callUserStatsReceived.m_taskID = CryLobbyInvalidTaskID;
		}
	}
}

////////////////////////////////////////////////////////////////////////////

void CCrySteamStats::EndStatsReadUserData(CryStatsTaskID sTaskID)
{
	STask* pTask = &m_task[sTaskID];
	if (pTask->error == eCLE_Success)
	{
		SCryLobbyUserData* pData = (SCryLobbyUserData*)m_pLobby->MemGetPtr(pTask->paramsMem[STATS_PARAM_WRITE_USERDATA_PTR]);
		((CryStatsReadUserDataCallback)pTask->pCb)(m_callUserStatsReceived.m_taskID, eCLE_Success, pData, m_numUserData, pTask->pCbArg);
	}
}

////////////////////////////////////////////////////////////////////////////

#endif // USE_STEAM && USE_CRY_STATS
