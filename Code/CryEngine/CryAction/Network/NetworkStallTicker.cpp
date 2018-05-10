// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameContext.h"
#include "NetworkStallTicker.h"

//--------------------------------------------------------------------------
// special ticker thread to run during load and unload of levels
#ifdef USE_NETWORK_STALL_TICKER_THREAD

	#define WARN_ABOUT_LONG_STALLS_IN_TICKER (0)  // Log warnings about long stalls in the loading ticker (which would likely cause disconnects)

void CNetworkStallTickerThread::ThreadEntry()
{
	bool gotLockLastTime = true;

	#if WARN_ABOUT_LONG_STALLS_IN_TICKER
	CTimeValue started = gEnv->pTimer->GetAsyncTime();
	CTimeValue ended;
	#endif

	while (m_threadRunning)
	{
		if (gEnv->pNetwork)
		{
			gEnv->pNetwork->SyncWithGame(eNGS_SleepNetwork);
		}

		if (gotLockLastTime)
		{
			CrySleep(33);
		}
		else
		{
			CrySleep(1);
		}

		{
			if (gEnv->pNetwork)
			{
				SCOPED_TICKER_TRY_LOCK;
				if (SCOPED_TICKER_HAS_LOCK)
				{
					gEnv->pNetwork->SyncWithGame(eNGS_MinimalUpdateForLoading);
					gotLockLastTime = true;
				}
				else
				{
					gotLockLastTime = false;
				}
				gEnv->pNetwork->SyncWithGame(eNGS_WakeNetwork);
			}

	#if WARN_ABOUT_LONG_STALLS_IN_TICKER
			ended = gEnv->pTimer->GetAsyncTime();
			if (ended.GetDifferenceInSeconds(started) > 1.f)
			{
				CryLogAlways("THREADEDLOADING:: No update for %f", ended.GetDifferenceInSeconds(started));
			}
			started = ended;
	#endif
		}
	}
}

#endif // #ifdef USE_NETWORK_STALL_TICKER_THREAD
//--------------------------------------------------------------------------
