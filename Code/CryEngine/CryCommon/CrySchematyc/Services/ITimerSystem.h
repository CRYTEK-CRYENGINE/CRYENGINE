// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Use scoped connections to automatically disconnect timers?
// #SchematycTODO : Use combination of pool and salt counters for more efficient look-up by id?

#pragma once

#include <CrySerialization/Forward.h>
#include <CryString/CryStringUtils.h>

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/EnumFlags.h"
#include "CrySchematyc/Utils/IString.h"
#include "CrySchematyc/Utils/PreprocessorUtils.h"
#include "CrySchematyc/Utils/StringUtils.h"

namespace Schematyc
{

enum class ETimerUnits
{
	Empty,
	Frames,
	Seconds,
	Random
};

struct STimerDuration
{
	inline STimerDuration()
		: units(ETimerUnits::Empty)
	{
		memset(this, 0, sizeof(STimerDuration));
	}

	explicit inline STimerDuration(uint32 _frames)
	{
		memset(this, 0, sizeof(STimerDuration));
		units = ETimerUnits::Frames;
		frames = _frames;
	}

	explicit inline STimerDuration(float _seconds)
	{
		memset(this, 0, sizeof(STimerDuration));
		units = ETimerUnits::Seconds;
		seconds = _seconds;
	}

	explicit inline STimerDuration(float _min, float _max)
	{
		memset(this, 0, sizeof(STimerDuration));
		units = ETimerUnits::Random;
		range.min = _min;
		range.max = _max;
	}

	inline STimerDuration(const STimerDuration& rhs)
	{
		memcpy(this, &rhs, sizeof(STimerDuration));
	}

	inline void ToString(IString& output) const
	{
		switch (units)
		{
		case ETimerUnits::Frames:
			{
				output.Format("%u(f)", frames);
				break;
			}
		case ETimerUnits::Seconds:
			{
				output.Format("%.4f(s)", seconds);
				break;
			}
		}
	}
	bool operator==( const STimerDuration &rhs ) const
	{
		return units == rhs.units && frames == rhs.frames && range.min == rhs.range.min && range.max == rhs.range.max;
	}

	static void ReflectType(CTypeDesc<STimerDuration>& desc)
	{
		desc.SetGUID("51008b69-77c5-42f8-9626-50e508e5581d"_cry_guid);
		desc.SetToStringOperator<&STimerDuration::ToString>();
	}

	ETimerUnits units;

	union
	{
		uint32 frames;
		float  seconds;

		struct
		{
			float min;
			float max;
		} range;
	};
};

enum class ETimerFlags
{
	None      = 0,
	AutoStart = BIT(0),
	Repeat    = BIT(1)
};

typedef CEnumFlags<ETimerFlags> TimerFlags;

inline void ReflectType(CTypeDesc<TimerFlags>& desc)
{
	desc.SetGUID("bbcb43b5-df7f-4298-a5d4-ea4413ba671e"_cry_guid);
}

struct STimerParams
{
	inline STimerParams(const STimerDuration& _duration = STimerDuration(), TimerFlags _flags = ETimerFlags::None)
		: duration(_duration)
		, flags(_flags)
	{}

	STimerDuration duration;
	TimerFlags     flags;
};

typedef std::function<void ()> TimerCallback;

enum class TimerId : uint32
{
	Invalid = 0xffffffff
};

inline bool Serialize(Serialization::IArchive& archive, TimerId& value, const char* szName, const char* szLabel)
{
	if (!archive.isEdit())
	{
		return archive(static_cast<uint32>(value), szName, szLabel);
	}
	return true;
}

struct ITimerSystem
{
	virtual ~ITimerSystem() {}

	virtual TimerId        CreateTimer(const STimerParams& params, const TimerCallback& callback) = 0;
	virtual void           DestroyTimer(const TimerId& timerId) = 0;
	virtual bool           StartTimer(const TimerId& timerId) = 0;
	virtual bool           StopTimer(const TimerId& timerId) = 0;
	virtual void           ResetTimer(const TimerId& timerId) = 0;
	virtual bool           IsTimerActive(const TimerId& timerId) const = 0;
	virtual STimerDuration GetTimeRemaining(const TimerId& timerId) const = 0;
	virtual void           Update() = 0;
};

} // Schematyc
