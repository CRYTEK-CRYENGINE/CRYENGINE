// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TimerSystem.h"

#include <CryMath/Random.h>
#include <CrySystem/ITimer.h>
#include <CrySchematyc/Utils/Assert.h>

namespace Schematyc
{
uint32 CTimerSystem::ms_nextTimerId = 1;

CTimerSystem::CTimerSystem()
	: m_frameCounter(0)
{}

TimerId CTimerSystem::CreateTimer(const STimerParams& params, const TimerCallback& callback)
{
	SCHEMATYC_CORE_ASSERT(callback);
	if (callback)
	{
		uint64 time = 0;
		const TimerId timerId = static_cast<TimerId>(ms_nextTimerId++);
		STimerDuration duration = params.duration;
		PrivateTimerFlags privateFlags = EPrivateTimerFlags::None;
		switch (duration.units)
		{
		case ETimerUnits::Frames:
			{
				time = m_frameCounter;
				break;
			}
		case ETimerUnits::Seconds:
			{
				time = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
				break;
			}
		case ETimerUnits::Random:
			{
				time = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
				duration.units = ETimerUnits::Seconds;
				duration.seconds = cry_random(duration.range.min, duration.range.max);
				break;
			}
		}
		if (params.flags.Check(ETimerFlags::AutoStart))
		{
			privateFlags.Add(EPrivateTimerFlags::Active);
		}
		if (params.flags.Check(ETimerFlags::Repeat))
		{
			privateFlags.Add(EPrivateTimerFlags::Repeat);
		}
		m_timers.push_back(STimer(time, timerId, duration, callback, privateFlags));
		return timerId;
	}
	return TimerId();
}

void CTimerSystem::DestroyTimer(const TimerId& timerId)
{
	Timers::iterator itEndTimer = m_timers.end();
	Timers::iterator itTimer = std::find_if(m_timers.begin(), itEndTimer, SEqualTimerId(timerId));
	if (itTimer != itEndTimer)
	{
		itTimer->privateFlags.Add(EPrivateTimerFlags::Destroy);
	}
}

bool CTimerSystem::StartTimer(const TimerId& timerId)
{
	Timers::iterator itEndTimer = m_timers.end();
	Timers::iterator itTimer = std::find_if(m_timers.begin(), itEndTimer, SEqualTimerId(timerId));
	if (itTimer != itEndTimer)
	{
		STimer& timer = *itTimer;
		if (!timer.privateFlags.Check(EPrivateTimerFlags::Active))
		{
			switch (timer.duration.units)
			{
			case ETimerUnits::Frames:
				{
					timer.time = m_frameCounter;
					break;
				}
			case ETimerUnits::Seconds:
				{
					timer.time = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
					break;
				}
			}
			timer.privateFlags.Add(EPrivateTimerFlags::Active);
			return true;
		}
	}
	return false;
}

bool CTimerSystem::StopTimer(const TimerId& timerId)
{
	Timers::iterator itEndTimer = m_timers.end();
	Timers::iterator itTimer = std::find_if(m_timers.begin(), itEndTimer, SEqualTimerId(timerId));
	if (itTimer != itEndTimer)
	{
		STimer& timer = *itTimer;
		if (timer.privateFlags.Check(EPrivateTimerFlags::Active))
		{
			timer.privateFlags.Remove(EPrivateTimerFlags::Active);
			return true;
		}
	}
	return false;
}

void CTimerSystem::ResetTimer(const TimerId& timerId)
{
	Timers::iterator itEndTimer = m_timers.end();
	Timers::iterator itTimer = std::find_if(m_timers.begin(), itEndTimer, SEqualTimerId(timerId));
	if (itTimer != itEndTimer)
	{
		STimer& timer = *itTimer;
		if (timer.privateFlags.Check(EPrivateTimerFlags::Active))
		{
			switch (timer.duration.units)
			{
			case ETimerUnits::Frames:
				{
					timer.time = m_frameCounter;
					break;
				}
			case ETimerUnits::Seconds:
			case ETimerUnits::Random:
				{
					timer.time = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
					break;
				}
			}
		}
	}
}

bool CTimerSystem::IsTimerActive(const TimerId& timerId) const
{
	Timers::const_iterator itEndTimer = m_timers.end();
	Timers::const_iterator itTimer = std::find_if(m_timers.begin(), itEndTimer, SEqualTimerId(timerId));
	if (itTimer != itEndTimer)
	{
		return itTimer->privateFlags.Check(EPrivateTimerFlags::Active);
	}
	return false;
}

STimerDuration CTimerSystem::GetTimeRemaining(const TimerId& timerId) const
{
	Timers::const_iterator itEndTimer = m_timers.end();
	Timers::const_iterator itTimer = std::find_if(m_timers.begin(), itEndTimer, SEqualTimerId(timerId));
	if (itTimer != itEndTimer)
	{
		const STimer& timer = *itTimer;
		if (timer.privateFlags.Check(EPrivateTimerFlags::Active))
		{
			switch (timer.duration.units)
			{
			case ETimerUnits::Frames:
				{
					const int64 time = m_frameCounter;
					const uint32 timeRemaining = timer.duration.frames - static_cast<uint32>(time - timer.time);
					return STimerDuration(std::max<uint32>(timeRemaining, 0));
				}
			case ETimerUnits::Seconds:
				{
					const int64 time = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
					const float timeRemaining = timer.duration.seconds - (static_cast<float>(time - timer.time) / 1000.0f);
					return STimerDuration(std::max<float>(timeRemaining, 0.0f));
				}
			}
		}
	}
	return STimerDuration();
}

void CTimerSystem::Update()
{
	const int64 frameCounter = m_frameCounter++;
	const int64 timeMs = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
	// Update active timers.
	uint32 timerCount = m_timers.size();
	for (uint32 timerIdx = 0; timerIdx < timerCount; ++timerIdx)
	{
		STimer& timer = m_timers[timerIdx];
		if (!timer.privateFlags.Check(EPrivateTimerFlags::Destroy))
		{
			if (timer.privateFlags.Check(EPrivateTimerFlags::Active))
			{
				int64 time = 0;
				int64 endTime = 0;
				switch (timer.duration.units)
				{
				case ETimerUnits::Frames:
					{
						time = frameCounter;
						endTime = timer.time + timer.duration.frames;
						break;
					}
				case ETimerUnits::Seconds:
					{
						time = timeMs;
						endTime = timer.time + static_cast<int64>((timer.duration.seconds * 1000.0f));
						break;
					}
				}
				if (time >= endTime)
				{
					timer.callback();
					if (timer.privateFlags.Check(EPrivateTimerFlags::Repeat))
					{
						timer.time = time;
					}
					else
					{
						timer.privateFlags.Remove(EPrivateTimerFlags::Active);
					}
				}
			}
		}
	}
	// Perform garbage collection.
	timerCount = m_timers.size();
	for (uint32 timerIdx = 0; timerIdx < timerCount; )
	{
		STimer& timer = m_timers[timerIdx];
		if (timer.privateFlags.Check(EPrivateTimerFlags::Destroy))
		{
			--timerCount;
			if (timerIdx < timerCount)
			{
				timer = m_timers[timerCount];
			}
		}
		++timerIdx;
	}
	m_timers.resize(timerCount);
}

CTimerSystem::STimer::STimer()
	: time(0)
	, privateFlags(EPrivateTimerFlags::None)
{}

CTimerSystem::STimer::STimer(int64 _time, const TimerId& _timerId, const STimerDuration& _duration, const TimerCallback& _callback, const PrivateTimerFlags& _privateFlags)
	: time(_time)
	, timerId(_timerId)
	, duration(_duration)
	, callback(_callback)
	, privateFlags(_privateFlags)
{}

CTimerSystem::STimer::STimer(const STimer& rhs)
	: time(rhs.time)
	, timerId(rhs.timerId)
	, duration(rhs.duration)
	, callback(rhs.callback)
	, privateFlags(rhs.privateFlags)
{}
} // Schematyc
