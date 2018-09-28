// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TimerSystem.h"

#include <CryMath/Random.h>
#include <CrySystem/ITimer.h>

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, ETimerUnits, "Schematyc Timer Units")
	SERIALIZATION_ENUM(Schematyc2::ETimerUnits::Frames, "Frames", "Frames")
	SERIALIZATION_ENUM(Schematyc2::ETimerUnits::Seconds, "Seconds", "Seconds")
	SERIALIZATION_ENUM(Schematyc2::ETimerUnits::Random, "Random", "Random")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN_NESTED(Schematyc2, ETimerFlags, "Schematyc Timer Flags")
	SERIALIZATION_ENUM(Schematyc2::ETimerFlags::AutoStart, "AutoStart", "Auto Start")
	SERIALIZATION_ENUM(Schematyc2::ETimerFlags::Repeat, "Repeat", "Repeat")
SERIALIZATION_ENUM_END()

namespace Schematyc2
{
	TimerId CTimerSystem::ms_nextTimerId = 1;

	//////////////////////////////////////////////////////////////////////////
	CTimerSystem::CTimerSystem()
		: m_frameCounter(0)
	{}

	//////////////////////////////////////////////////////////////////////////
	TimerId CTimerSystem::CreateTimer(const STimerParams& params, const TimerCallback& callback)
	{
		CRY_ASSERT(callback);
		if(callback)
		{
			CTimeValue				time = 0;
			const TimerId			timerId = ms_nextTimerId ++;
			STimerDuration			duration = params.duration;
			EPrivateTimerFlags	privateFlags = EPrivateTimerFlags::None;
			switch(duration.units)
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
					time					= GetGTimer()->GetFrameStartTime();
					duration.units		= ETimerUnits::Seconds;
					duration.seconds	= cry_random(duration.range.min, duration.range.max);
					break;
				}
			}
			if((params.flags & ETimerFlags::AutoStart) != 0)
			{
				privateFlags |= EPrivateTimerFlags::Active;
			}
			if((params.flags & ETimerFlags::Repeat) != 0)
			{
				privateFlags |= EPrivateTimerFlags::Repeat;
			}
			m_timers.push_back(STimer(time, timerId, duration, callback, privateFlags));
			return timerId;
		}
		return s_invalidTimerId;
	}

	//////////////////////////////////////////////////////////////////////////
	void CTimerSystem::DestroyTimer(TimerId timerId)
	{
		TimerVector::iterator	iEndTimer = m_timers.end();
		TimerVector::iterator	iTimer = std::find_if(m_timers.begin(), iEndTimer, SEqualTimerId(timerId));
		if(iTimer != iEndTimer)
		{
			iTimer->privateFlags |= EPrivateTimerFlags::Destroy;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CTimerSystem::StartTimer(TimerId timerId)
	{
		TimerVector::iterator	iEndTimer = m_timers.end();
		TimerVector::iterator	iTimer = std::find_if(m_timers.begin(), iEndTimer, SEqualTimerId(timerId));
		if(iTimer != iEndTimer)
		{
			STimer&	timer = *iTimer;
			if((timer.privateFlags & EPrivateTimerFlags::Active) == 0)
			{
				switch(timer.duration.units)
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
				timer.privateFlags |= EPrivateTimerFlags::Active;
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	bool CTimerSystem::StopTimer(TimerId timerId)
	{
		TimerVector::iterator	iEndTimer = m_timers.end();
		TimerVector::iterator	iTimer = std::find_if(m_timers.begin(), iEndTimer, SEqualTimerId(timerId));
		if(iTimer != iEndTimer)
		{
			STimer&	timer = *iTimer;
			if((timer.privateFlags & EPrivateTimerFlags::Active) != 0)
			{
				timer.privateFlags &= ~EPrivateTimerFlags::Active;
				return true;
			}
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CTimerSystem::ResetTimer(TimerId timerId)
	{
		TimerVector::iterator	iEndTimer = m_timers.end();
		TimerVector::iterator	iTimer = std::find_if(m_timers.begin(), iEndTimer, SEqualTimerId(timerId));
		if(iTimer != iEndTimer)
		{
			STimer&	timer = *iTimer;
			if((timer.privateFlags & EPrivateTimerFlags::Active) != 0)
			{
				switch(timer.duration.units)
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

	//////////////////////////////////////////////////////////////////////////
	bool CTimerSystem::IsTimerActive(TimerId timerId) const
	{
		TimerVector::const_iterator	iEndTimer = m_timers.end();
		TimerVector::const_iterator	iTimer = std::find_if(m_timers.begin(), iEndTimer, SEqualTimerId(timerId));
		if(iTimer != iEndTimer)
		{
			return (iTimer->privateFlags & EPrivateTimerFlags::Active) != 0;
		}
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	STimerDuration CTimerSystem::GetTimeRemaining(TimerId timerId) const
	{
		TimerVector::const_iterator	iEndTimer = m_timers.end();
		TimerVector::const_iterator	iTimer = std::find_if(m_timers.begin(), iEndTimer, SEqualTimerId(timerId));
		if(iTimer != iEndTimer)
		{
			const STimer&	timer = *iTimer;
			if((timer.privateFlags & EPrivateTimerFlags::Active) != 0)
			{
				switch(timer.duration.units)
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
						const CTimeValue	time = GetGTimer()->GetFrameStartTime();
						const CTimeValue	timeRemaining = timer.duration.seconds - (time - timer.time);
						return STimerDuration(std::max(timeRemaining, CTimeValue(0)));
					}
				}
			}
		}
		return STimerDuration();
	}

	//////////////////////////////////////////////////////////////////////////
	void CTimerSystem::Update()
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		const int64	frameCounter = m_frameCounter ++;
		const CTimeValue timeCur = GetGTimer()->GetFrameStartTime();
		// Update active timers.
		size_t	timerCount = m_timers.size();
		for(size_t iTimer = 0; iTimer < timerCount; ++ iTimer)
		{
			STimer&	timer = m_timers[iTimer];
			if((timer.privateFlags & EPrivateTimerFlags::Destroy) == 0)
			{
				if((timer.privateFlags & EPrivateTimerFlags::Active) != 0)
				{
					CTimeValue	time = 0;
					CTimeValue	endTime = 0;
					switch(timer.duration.units)
					{
					case ETimerUnits::Frames:
						{
							time		= CTimeValue(frameCounter);
							endTime	= timer.time + timer.duration.frames;
							break;
						}
					case ETimerUnits::Seconds:
						{
							time		= timeCur;
							endTime	= timer.time + timer.duration.seconds;
							break;
						}
					}
					if(time >= endTime)
					{
						timer.callback();
						if((timer.privateFlags & EPrivateTimerFlags::Repeat) != 0)
						{
							timer.time = time;
						}
						else
						{
							timer.privateFlags &= ~EPrivateTimerFlags::Active;
						}
					}
				}
			}
		}
		// Perform garbage collection.
		timerCount = m_timers.size();
		for(size_t iTimer = 0; iTimer < timerCount; )
		{
			STimer&	timer = m_timers[iTimer];
			if((timer.privateFlags & EPrivateTimerFlags::Destroy) != 0)
			{
				-- timerCount;
				if(iTimer < timerCount)
				{
					timer = m_timers[timerCount];
				}
			}
			++ iTimer;
		}
		m_timers.resize(timerCount);
	}

	//////////////////////////////////////////////////////////////////////////
	CTimerSystem::STimer::STimer()
		: time(0)
		, timerId(s_invalidTimerId)
		, privateFlags(EPrivateTimerFlags::None)
	{}

	//////////////////////////////////////////////////////////////////////////
	CTimerSystem::STimer::STimer(const CTimeValue& _time, TimerId _timerId, const STimerDuration& _duration, const TimerCallback& _callback, EPrivateTimerFlags _privateFlags)
		: time(_time)
		, timerId(_timerId)
		, duration(_duration)
		, callback(_callback)
		, privateFlags(_privateFlags)
	{}

	//////////////////////////////////////////////////////////////////////////
	CTimerSystem::STimer::STimer(const STimer& rhs)
		: time(rhs.time)
		, timerId(rhs.timerId)
		, duration(rhs.duration)
		, callback(rhs.callback)
		, privateFlags(rhs.privateFlags)
	{}
}
