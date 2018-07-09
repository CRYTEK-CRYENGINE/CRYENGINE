// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

enum ECryRewardError
{
	eCRE_Queued = 0,     //!< Reward successfully queued.
	eCRE_Busy,           //!< Reward queue full - try again later.
	eCRE_Failed          //!< Reward process failed.
};

//! \param taskID		Task ID allocated when the function was executed.
//! \param error		Error code - eCLE_Success if the function succeeded or an error that occurred while processing the function.
//! \param pArg			Pointer to application-specified data.
typedef void (* CryRewardCallback)(CryLobbyTaskID taskID, uint32 playerID, uint32 awardID, ECryLobbyError error, bool alreadyAwarded, void* pArg);

struct ICryReward
{
	// <interfuscator:shuffle>
	virtual ~ICryReward(){}

	//! Awards an achievement/trophy/reward to the specified player.
	//! \param playerID	Player ID.
	//! \param awardID	Award ID (probably implemented as an enumerated type).
	//! \return Informs the caller that the award was added to the pending queue or not.
	virtual ECryRewardError Award(uint32 playerID, uint32 awardID, CryLobbyTaskID* pTaskID, CryRewardCallback cb, void* pCbArg) = 0;
	// </interfuscator:shuffle>
};
