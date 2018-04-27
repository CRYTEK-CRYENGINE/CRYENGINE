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
			uint64							time = 0;
			const TimerId				timerId = ms_nextTimerId ++;
			STimerDuration			duration = params.duration;
			EPrivateTimerFlags	privateFlags = EPrivateTimerFlags::None;
			switch(duration.units)
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
					time							= gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
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
						timer.time = m_frameCounter;
						break;
					}
				case ETimerUnits::Seconds:
					{
						timer.time = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
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
						const int64		time = m_frameCounter;
						const uint32	timeRemaining = timer.duration.frames - static_cast<uint32>(time - timer.time);
						return STimerDuration(std::max<uint32>(timeRemaining, 0));
					}
				case ETimerUnits::Seconds:
					{
						const int64	time = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
						const float	timeRemaining = timer.duration.seconds - (static_cast<float>(time - timer.time) / 1000.0f);
						return STimerDuration(std::max<float>(timeRemaining, 0.0f));
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
		const int64	timeMs = gEnv->pTimer->GetFrameStartTime().GetMilliSecondsAsInt64();
		// Update active timers.
		size_t	timerCount = m_timers.size();
		for(size_t iTimer = 0; iTimer < timerCount; ++ iTimer)
		{
			STimer&	timer = m_timers[iTimer];
			if((timer.privateFlags & EPrivateTimerFlags::Destroy) == 0)
			{
				if((timer.privateFlags & EPrivateTimerFlags::Active) != 0)
				{
					int64	time = 0;
					int64	endTime = 0;
					switch(timer.duration.units)
					{
					case ETimerUnits::Frames:
						{
							time		= frameCounter;
							endTime	= timer.time + timer.duration.frames;
							break;
						}
					case ETimerUnits::Seconds:
						{
							time		= timeMs;
							endTime	= timer.time + static_cast<int64>((timer.duration.seconds * 1000.0f));
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
	CTimerSystem::STimer::STimer(int64 _time, TimerId _timerId, const STimerDuration& _duration, const TimerCallback& _callback, EPrivateTimerFlags _privateFlags)
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
