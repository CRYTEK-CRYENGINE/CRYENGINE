// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   CrySteamReward.h
//  Version:     v1.00
//  Created:     21/01/2013 by MichielM.
//  Compilers:   Visual Studio.NET 2010
//  Description: Handles reward/achievements for the Steam API
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#if !defined(__CRYSTEAMREWARD_H__)
#define __CRYSTEAMREWARD_H__

#if USE_STEAM

	#include "CryReward.h"
	#include "CrySteamMatchMaking.h"

	#define MAXIMUM_PENDING_ACHIEVEMENTS 8
	#define NUMBER_OF_ACHIEVEMENTS       5
	#define ACHIEVEMENT_NAME_LENGTH      128
	#define ACHIEVEMENT_DESC_LENGTH      256
	#define MAX_REWARD_TASKS             8
	#define REWARD_TASK_TIMEOUT          5.0f

	#define _ACH_ID(id, name) { "", name, id, # id, 0, 0 }

/*
   There can only be 1 player logged into steam on a single PC
   Therefore we don't have to worry about different users in achievements (in the same instance of launcher)
   So keeping 1 struct synced for the achievements is enough
 */

typedef uint32 CrySteamRewardTaskId;

struct CrySteamReward
{
	char        m_sDescription[ACHIEVEMENT_DESC_LENGTH];
	char        m_sName[ACHIEVEMENT_NAME_LENGTH];
	int         m_eAchievementID;
	const char* m_pchAchievementID;
	int         m_iIconImage;
	bool        m_bAchieved;
};

enum ETask
{
	eT_GetSteamData,
	eT_InsertAward,
	eT_ClearAwards,
};

class CCrySteamReward : public CCryReward
{
public:
	CCrySteamReward(CCryLobby* pLobby);
	virtual ~CCrySteamReward(void);

	virtual ECryRewardError Award(uint32 playerID, uint32 awardID, CryLobbyTaskID* pTaskID, CryRewardCallback cb, void* pCbArg);

	virtual void            Tick(CTimeValue tv);

	ECryLobbyError          Initialise();
	ECryLobbyError          Terminate() { return eCLE_Success; }

protected:

	CRYSTEAM_CALLBACK(CCrySteamReward, OnUserStatsReceived, UserStatsReceived_t, m_CallbackUserStatsReceived);
	CRYSTEAM_CALLBACK(CCrySteamReward, OnUserStatsStored, UserStatsStored_t, m_CallbackUserStatsStored);
	CRYSTEAM_CALLBACK(CCrySteamReward, OnAchievementStored, UserAchievementStored_t, m_CallbackAchievementStored);

	void StartAward(uint32 awardID, CryLobbyTaskID lTaskID, CryRewardCallback cb, void* pCbArg);

	bool RequestStats();
	bool InternalAward(int achievementID);

	#if !defined(_RELEASE)
	bool ClearAwards(CrySteamRewardTaskId taskId);
	#endif

	ECryLobbyError StartTask(ETask task, CryLobbyTaskID id, CrySteamRewardTaskId& rewardTaskID);
	void           EndTask(CrySteamRewardTaskId taskID);
	void           TickTasks(CTimeValue tv);

	//eT_GetSteamData
	ECryLobbyError StartTaskGetSteamData();
	void           StartGetSteamData(CrySteamRewardTaskId task);            // ask steam for it
	void           EndGetSteamData(CrySteamRewardTaskId task, ECryLobbyError error);

	//eT_InsertAward
	void StartAddAwardToPending(CrySteamRewardTaskId task);
	void EndAddAwardToPending(CrySteamRewardTaskId task, ECryLobbyError error);

	#if !defined(_RELEASE)
	//eT_ClearAwards
	void StartClearAwards(CrySteamRewardTaskId task);
	void EndClearAwards(CrySteamRewardTaskId task, ECryLobbyError error);
	#endif

protected:
	struct SPendingAchievement
	{
		CryLobbyTaskID    lTaskID;
		CryRewardCallback pCb;
		void*             pCbArg;
		uint32            achievementID;
	} m_pendingAchievements[MAXIMUM_PENDING_ACHIEVEMENTS];

	struct STask
	{
		CTimeValue        m_StartTime;
		CryLobbyTaskID    lTaskID;
		ETask             m_eStartedTask;
		CryRewardCallback pCb;
		void*             pCbArg;
		uint32            achievementID;
		ECryLobbyError    error;
		bool              m_bFinished;
		bool              m_bUsed;
		bool              m_bRunning;
	};

	CrySteamReward* m_achievements;
	int             m_iNumAchievements;
	bool            m_bInitialized;
	int             m_count;

	STask           m_tasks[MAX_REWARD_TASKS];

	uint32          m_taskQueue[MAX_REWARD_TASKS];           //store indices from the m_tasks queue (sorted version of m_tasks)

protected:
	void TriggerCallbackOnGameThread(STask task);

};

#endif // USE_STEAM
#endif // End [!defined(__CRYSTEAMREWARD_H__)]
