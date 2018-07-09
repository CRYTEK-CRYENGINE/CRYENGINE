// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Restarting random timers will not affect duration. Is this an issue?

#pragma once

#include <CrySchematyc/Services/ITimerSystem.h>
#include <CrySchematyc/Utils/EnumFlags.h>

namespace Schematyc
{
enum class EPrivateTimerFlags
{
	None    = 0,
	Active  = BIT(0),
	Repeat  = BIT(1),
	Destroy = BIT(2)
};

typedef CEnumFlags<EPrivateTimerFlags> PrivateTimerFlags;

class CTimerSystem : public ITimerSystem
{
private:

	struct STimer
	{
		STimer();
		STimer(int64 _time, const TimerId& _timerId, const STimerDuration& _duration, const TimerCallback& _callback, const PrivateTimerFlags& _privateFlags);
		STimer(const STimer& rhs);

		int64             time;
		TimerId           timerId;
		STimerDuration    duration;
		TimerCallback     callback;
		PrivateTimerFlags privateFlags;
	};

	struct SEqualTimerId
	{
		inline SEqualTimerId(const TimerId& _timerId)
			: timerId(_timerId)
		{}

		inline bool operator()(const STimer& timer) const
		{
			return timer.timerId == timerId;
		}

		TimerId timerId;
	};

	typedef std::vector<STimer> Timers;

public:

	CTimerSystem();

	// ITimerSystem
	virtual TimerId        CreateTimer(const STimerParams& params, const TimerCallback& callback) override;
	virtual void           DestroyTimer(const TimerId& timerId) override;
	virtual bool           StartTimer(const TimerId& timerId) override;
	virtual bool           StopTimer(const TimerId& timerId) override;
	virtual void           ResetTimer(const TimerId& timerId) override;
	virtual bool           IsTimerActive(const TimerId& timerId) const override;
	virtual STimerDuration GetTimeRemaining(const TimerId& timerId) const override;
	virtual void           Update() override;
	// ~ITimerSystem

private:

	static uint32 ms_nextTimerId;

private:

	int64  m_frameCounter;
	Timers m_timers;
};
} // Schematyc
