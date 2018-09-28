// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once
#include <CryThreading/IThreadManager.h>

//! Runs in its own thread to watch over game freezes longer than user specified time out.
//! Turns time out into fatal error.
class CWatchdogThread : public IThread
{
public:

	void SignalStopWork();

	//! Creates and starts running watchdog thread.
	//! \param timeOutSeconds	time out value in seconds, must be positive
	explicit CWatchdogThread(const CTimeValue& timeOut);
	~CWatchdogThread();

	//! Changes time out value.
	//! \param timeOutSeconds	time out value in seconds, must be positive
	void SetTimeout(const CTimeValue& timeOut);
private:

	static uint64 GetSystemUpdateCounter();
	void          Sleep() const;

	virtual void  ThreadEntry() override;

	volatile bool m_bQuit = false;
	CTimeValue    m_timeOut = 0;
};
