// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CallbackTimer.h"

#ifndef _RELEASE
	#define EnsureMainThread()                                                                                   \
	  if (gEnv->mMainThreadId != CryGetCurrentThreadId()) {                                                      \
	    CryFatalError("%s called from invalid thread: %" PRI_THREADID "! Expected %" PRI_THREADID, __FUNCTION__, \
	                  CryGetCurrentThreadId(), gEnv->mMainThreadId); }
#else
	#define EnsureMainThread() (void)0
#endif

const size_t MaxTimerCount = 256;

CallbackTimer::CallbackTimer()
	: m_resort(false)
	, m_timers(MaxTimerCount)
{
}

void CallbackTimer::Clear()
{
	m_resort = false;
	Timeouts().swap(m_timeouts);
	Timers(MaxTimerCount).swap(m_timers);
}

void CallbackTimer::Update()
{
	if (!m_timeouts.empty())
	{
		if (m_resort)
		{
			std::sort(m_timeouts.begin(), m_timeouts.end());
			m_resort = false;
		}

		CTimeValue now = gEnv->pTimer->GetFrameStartTime();

		while (!m_timeouts.empty() && (m_timeouts.front().timeout <= now))
		{
			TimeoutInfo timeout = m_timeouts.front();
			m_timeouts.pop_front();

			TimerInfo timer = m_timers[timeout.timerID];

			timer.callback(timer.userdata, timeout.timerID);

			if (m_timers.validate(timeout.timerID))
			{
				if (!timer.repeating)
					m_timers.erase(timeout.timerID);
				else
				{
					CTimeValue nextTimeout = timeout.timeout + timer.interval;
					if (nextTimeout.GetValue() <= now.GetValue())
						nextTimeout.SetValue(now.GetValue() + 1);

					timeout.timeout = nextTimeout;
					m_timeouts.push_back(timeout);
					m_resort = true;
				}
			}
		}
	}
}

CallbackTimer::TimerID CallbackTimer::AddTimer(CTimeValue interval, bool repeating, const Callback& callback, void* userdata)
{
	EnsureMainThread();

	CTimeValue now = gEnv->pTimer->GetFrameStartTime();

	if (m_timers.full())
	{
		GameWarning("%s Maximum number of timers (%" PRISIZE_T ") exceeded!", __FUNCTION__, m_timers.capacity());
		m_timers.grow(8); // little growth to keep the warnings going - update MaxTimerCount if necessary
	}

	TimerID timerID = m_timers.insert(TimerInfo(interval, repeating, callback, userdata));
	m_timeouts.push_back(TimeoutInfo(now + interval, timerID));
	m_resort = true;

	return timerID;
}

void* CallbackTimer::RemoveTimer(TimerID timerID)
{
	EnsureMainThread();

	if (m_timers.validate(timerID))
	{
		void* userdata = m_timers[timerID].userdata;
		m_timers.erase(timerID);

		Timeouts::iterator it = m_timeouts.begin();
		Timeouts::iterator end = m_timeouts.end();

		for (; it != end; ++it)
		{
			TimeoutInfo& timeout = *it;

			if (timeout.timerID == timerID)
			{
				std::swap(timeout, m_timeouts.front());
				m_timeouts.pop_front();
				m_resort = true;

				return userdata;
			}
		}
	}

	return 0;
}

void CallbackTimer::GetMemoryStatistics(ICrySizer* sizer)
{
	sizer->Add(*this);
	sizer->Add(m_timers);
	sizer->Add(m_timeouts);
}
