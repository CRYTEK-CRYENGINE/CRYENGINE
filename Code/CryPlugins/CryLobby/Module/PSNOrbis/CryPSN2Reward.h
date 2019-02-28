// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __CRYPSN2_REWARD_H__
#define __CRYPSN2_REWARD_H__
#pragma once

#if CRY_PLATFORM_ORBIS
	#if USE_PSN

		#include <CryCore/Platform/IPlatformOS.h>
		#include "CryReward.h"
		#include <np.h>

		#define MAXIMUM_PENDING_TROPHIES (8)
		#define NUM_TROPHY_USERS         1

class CCryPSNReward : public CCryReward
{

public:
	CCryPSNReward(CCryLobby* pLobby);
	virtual ~CCryPSNReward(void);

	virtual void            Tick(CTimeValue tv);

	ECryLobbyError          Initialise();
	ECryLobbyError          Terminate();

	virtual ECryRewardError Award(uint32 user, uint32 awardID, CryLobbyTaskID* pTaskID, CryRewardCallback cb, void* pCbArg);

	static ECryLobbyError   RegisterTrophies(uint32 user, void* pArg);

protected:

	struct SRegisterTrophiesData
	{
		int error;
	};

	struct SAwardTrophyData
	{
		SceNpTrophyId awardedTrophyId;
		SceNpTrophyId platinumTrophyId;
		int           error;
	};

	struct STrophyUser
	{
		SRegisterTrophiesData  m_RegisterTrophiesWorkArea;
		SRegisterTrophiesData* m_pRegisteredTrophiesResult;
		SAwardTrophyData       m_AwardTrophyWorkArea;
		SAwardTrophyData*      m_pAwardedTrophyResult;
		SceNpTrophyContext     m_trophyContext;
		SceNpTrophyHandle      m_trophyHandle;
		uint32                 m_user;

		ECryLobbyError Initialise(uint32 user);
		ECryLobbyError Reset();
		ECryLobbyError CreateHandle();
		void           ClearRegisterTrophiesWorkArea();
		void           ClearAwardTrophyWorkArea();
		static void    TrophyThreadRegisterTrophiesFunc(CCryPSNReward::STrophyUser* pTrophyUser, uint64 data1);
		static void    TrophyThreadAwardTrophyFunc(CCryPSNReward::STrophyUser* pTrophyUser, uint64 data1);
	};

	struct SPendingTrophy
	{
		CryRewardCallback pCb;
		void*             pCbArg;
		CryLobbyTaskID    lTaskID;
		SceNpTrophyId     trophyID;
		uint32            user;
	};

	typedef void (* TOrbisTrophyThreadJobFunc)(STrophyUser*, uint64);

	struct SOrbisTrophyThreadJobInfo
	{
		TOrbisTrophyThreadJobFunc   m_pJobFunc;
		CCryPSNReward::STrophyUser* m_pTrophyUser;
		uint64                      m_data1;

		SOrbisTrophyThreadJobInfo(TOrbisTrophyThreadJobFunc pFunc, CCryPSNReward::STrophyUser* pTrophyUser, uint64 data1)
		{
			m_pJobFunc = pFunc;
			m_pTrophyUser = pTrophyUser;
			m_data1 = data1;
		}
	};

	class COrbisTrophyThread
		: public IThread
	{
	public:
		COrbisTrophyThread() : m_bAlive(true) {};

		// Start accepting work on thread
		virtual void ThreadEntry();           // IThread
		void         SignalStopWork();

		void         Push(TOrbisTrophyThreadJobFunc pFunc, CCryPSNReward::STrophyUser* pTrophyUser, uint64 data1);
		void         Flush(CCryPSNReward::STrophyUser* pTrophyUser);

	private:
		CryMutex                              m_mtx;
		CryConditionVariable                  m_cond;
		std::deque<SOrbisTrophyThreadJobInfo> m_jobQueue;
		bool m_bAlive;
	};

	COrbisTrophyThread* m_pOrbisTrophyJobThread;

	void         TriggerCallbackOnGameThread(SPendingTrophy pending, ECryLobbyError error);
	STrophyUser* GetTrophyUserFromUser(uint32 user);

	STrophyUser    m_User[NUM_TROPHY_USERS];

	SPendingTrophy m_pendingTrophies[MAXIMUM_PENDING_TROPHIES];
	uint32         m_pendingTrophyCount;
};

	#endif // End [USE_PSN]
#endif   // CRY_PLATFORM_ORBIS

#endif // End [!defined(__CRYPSN2_REWARD_H__)]
// [EOF]
