// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryReward.h"

CCryReward::CCryReward(CCryLobby* pLobby)
	: m_pLobby(pLobby)
{
	assert(pLobby);
	m_status = eCRS_Uninitialised;
}

CCryReward::~CCryReward(void)
{
}

ECryRewardError CCryReward::Award(uint32 playerID, uint32 awardID, CryLobbyTaskID* pTaskID, CryRewardCallback cb, void* pCbArg)
{
	// Default implementation
	return eCRE_Failed;
}

void CCryReward::Tick(CTimeValue tv)
{
}
// [EOF]
