// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETWORK_STALL_TICKER_H__
#define __NETWORK_STALL_TICKER_H__

#pragma once

//--------------------------------------------------------------------------
// a special ticker thread to run during load and unload of levels

#ifdef USE_NETWORK_STALL_TICKER_THREAD

	#include <CrySystem/ISystem.h>
	#include <CrySystem/ICmdLine.h>

	#include <CryGame/IGameFramework.h>
	#include <CryThreading/IThreadManager.h>

class CNetworkStallTickerThread : public IThread  //in multiplayer mode
{
public:
	CNetworkStallTickerThread()
	{
		m_threadRunning = true;
	}

	// Start accepting work on thread
	virtual void ThreadEntry();

	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork()
	{
		m_threadRunning = false;
	}

private:
	volatile bool m_threadRunning;
};

#endif // #ifdef USE_NETWORK_STALL_TICKER_THREAD
//--------------------------------------------------------------------------

#endif
