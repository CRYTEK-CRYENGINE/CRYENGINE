// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Restarting random timers will not affect duration. Is this an issue?

#pragma once

#include <CrySchematyc2/Services/ITimerSystem.h>

namespace Schematyc2
{
	enum class EPrivateTimerFlags
	{
		None    = 0,
		Active  = BIT(0),
		Repeat  = BIT(1),
		Destroy = BIT(2)
	};

	DECLARE_ENUM_CLASS_FLAGS(EPrivateTimerFlags)

	// Timer system.
	////////////////////////////////////////////////////////////////////////////////////////////////////
	class CTimerSystem : public ITimerSystem
	{
	public:

		CTimerSystem();

		// ITimerSystem
		virtual TimerId CreateTimer(const STimerParams& params, const TimerCallback& callback) override;
		virtual void DestroyTimer(TimerId timerId) override;
		virtual bool StartTimer(TimerId timerId) override;
		virtual bool StopTimer(TimerId timerId) override;
		virtual void ResetTimer(TimerId timerId) override;
		virtual bool IsTimerActive(TimerId timerId) const override;
		virtual STimerDuration GetTimeRemaining(TimerId timerId) const override;
		virtual void Update() override;
		//virtual bool DurationToString(const STimerDuration& duration, const TCharArray& output) override;
		// ~ITimerSystem

	private:

		struct STimer
		{
			STimer();
			STimer(int64 _time, TimerId _timerId, const STimerDuration& _duration, const TimerCallback& _callback, EPrivateTimerFlags _privateFlags);
			STimer(const STimer& rhs);

			int64              time;
			TimerId            timerId;
			STimerDuration     duration;
			TimerCallback      callback;
			EPrivateTimerFlags privateFlags;
		};

		struct SEqualTimerId
		{
			inline SEqualTimerId(TimerId _timerId)
				: timerId(_timerId)
			{}

			inline bool operator () (const STimer& timer) const
			{
				return timer.timerId == timerId;
			}

			TimerId	timerId;
		};

		typedef std::vector<STimer> TimerVector;

		static TimerId ms_nextTimerId;

		int64       m_frameCounter;
		TimerVector m_timers;
	};
}
