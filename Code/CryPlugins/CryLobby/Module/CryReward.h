// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(__CRYREWARD_H__)
#define __CRYREWARD_H__

#include <CryLobby/ICryReward.h>
#include "CryLobby.h"

enum ECryRewardStatus
{
	eCRS_Uninitialised = 0, // Reward system is uninitialised
	eCRS_Initialising,      // Reward system is currently being initialised
	eCRS_Idle,              // Reward system is idle.
	eCRS_Working,           // Reward system is working: currently unlocking some awards
};

class CCryReward : public CMultiThreadRefCount, public ICryReward
{
public:
	CCryReward(CCryLobby* pLobby);
	virtual ~CCryReward(void);

	// ICryReward
	virtual ECryRewardError Award(uint32 playerID, uint32 awardID, CryLobbyTaskID* pTaskID, CryRewardCallback cb, void* pCbArg);

	virtual void            Tick(CTimeValue tv);

	virtual bool            IsDead() const { return false; }

protected:
	ECryRewardStatus m_status;
	CCryLobby*       m_pLobby;
};

#endif // End [!defined(__CRYREWARD_H__)]
// [EOF]
