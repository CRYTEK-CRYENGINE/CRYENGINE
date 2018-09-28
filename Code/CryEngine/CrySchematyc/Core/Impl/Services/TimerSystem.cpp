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
		CTimeValue time;
		const TimerId timerId = static_cast<TimerId>(ms_nextTimerId++);
		STimerDuration duration = params.duration;
		PrivateTimerFlags privateFlags = EPrivateTimerFlags::None;
		switch (duration.units)
		{
		case ETimerUnits::Frames:
			{
				time = CTimeValue(m_frameCounter);
				break;
			}
		case ETimerUnits::Seconds:
			{
				time = GetGTimer()->GetFrameStartTime();
				break;
			}
		case ETimerUnits::Random:
			{
				time = GetGTimer()->GetFrameStartTime();
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
					timer.time = CTimeValue(m_frameCounter);
					break;
				}
			case ETimerUnits::Seconds:
				{
					timer.time = GetGTimer()->GetFrameStartTime();
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
					timer.time = CTimeValue(m_frameCounter);
					break;
				}
			case ETimerUnits::Seconds:
			case ETimerUnits::Random:
				{
					timer.time = GetGTimer()->GetFrameStartTime();
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
					// PERSONAL NOTE: Treating frame count's as CTimeValue, e.g. 1 frame = 1 second......slightly messier but preserves the older format.
					const int64  time = m_frameCounter;
					const int64  timeRemaining = timer.duration.frames - (time - (uint32)timer.time.GetSeconds());
					return STimerDuration().Frames(std::max<uint32>((uint32)timeRemaining, 0));
				}
			case ETimerUnits::Seconds:
				{
					const CTimeValue time = GetGTimer()->GetFrameStartTime();
					const CTimeValue timeRemaining = timer.duration.seconds - (time - timer.time);
					return STimerDuration(std::max(timeRemaining, CTimeValue(0)));
				}
			}
		}
	}
	return STimerDuration();
}

void CTimerSystem::Update()
{
	const int64 frameCounter = m_frameCounter++;
	const CTimeValue timeNow = GetGTimer()->GetFrameStartTime();
	// Update active timers.
	uint32 timerCount = m_timers.size();
	for (uint32 timerIdx = 0; timerIdx < timerCount; ++timerIdx)
	{
		STimer& timer = m_timers[timerIdx];
		if (!timer.privateFlags.Check(EPrivateTimerFlags::Destroy))
		{
			if (timer.privateFlags.Check(EPrivateTimerFlags::Active))
			{
				CTimeValue time;
				CTimeValue endTime;
				switch (timer.duration.units)
				{
				case ETimerUnits::Frames:
					{
						time = CTimeValue(frameCounter);
						endTime = timer.time + timer.duration.frames;
						break;
					}
				case ETimerUnits::Seconds:
					{
						time = timeNow;
						endTime = timer.time + timer.duration.seconds;
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

CTimerSystem::STimer::STimer(const CTimeValue& _time, const TimerId& _timerId, const STimerDuration& _duration, const TimerCallback& _callback, const PrivateTimerFlags& _privateFlags)
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
