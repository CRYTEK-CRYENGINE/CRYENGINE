// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _ITIMER_H_
#define _ITIMER_H_

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include "TimeValue.h"        // CTimeValue
#include <CryNetwork/SerializeFwd.h>

struct tm;

//! Interface to the Timer System.
struct ITimer
{
	enum ETimer
	{
		ETIMER_GAME = 0, //!< Pausable, serialized, frametime is smoothed/scaled/clamped.
		ETIMER_UI,       //!< Non-pausable, non-serialized, frametime unprocessed.
		ETIMER_LAST
	};

	enum ETimeScaleChannels
	{
		eTSC_Trackview = 0,
		eTSC_GameStart
	};

	// <interfuscator:shuffle>
	virtual ~ITimer() {};

	//! Resets the timer
	//! \note Only needed because float precision wasn't last that long - can be removed if 64bit is used everywhere.
	virtual void ResetTimer() = 0;

	//! Updates the timer every frame, needs to be called by the system.
	virtual void UpdateOnFrameStart() = 0;

	// todo: Remove, use GetFrameStartTime() instead.
	//! Returns the absolute time at the last UpdateOnFrameStart() call.
	virtual float GetCurrTime(ETimer which = ETIMER_GAME) const = 0;

	//! Returns the absolute time at the last UpdateOnFrameStart() call.
	virtual const CTimeValue& GetFrameStartTime(ETimer which = ETIMER_GAME) const = 0;

	//! Returns the absolute current time.
	//! \note The value continuously changes, slower than GetFrameStartTime().
	virtual CTimeValue GetAsyncTime() const = 0;

	//! Returns the absolute current time at the moment of the call.
	virtual float GetAsyncCurTime() = 0;

	//! Returns sum of relative time passed at each frame.
	virtual float GetReplicationTime() const = 0;

	//! Returns the relative time passed from the last UpdateOnFrameStart() in seconds.
	virtual float GetFrameTime(ETimer which = ETIMER_GAME) const = 0;

	//! Returns the relative time passed from the last UpdateOnFrameStart() in seconds without any dilation, smoothing, clamping, etc...
	virtual float GetRealFrameTime() const = 0;

	//! Returns the time scale applied to time values.
	virtual float GetTimeScale() const = 0;

	//! Returns the time scale factor for the given channel
	virtual float GetTimeScale(uint32 channel) const = 0;

	//! Clears all current time scale requests
	virtual void ClearTimeScales() = 0;

	//! Sets the time scale applied to time values.
	virtual void SetTimeScale(float s, uint32 channel = 0) = 0;

	//! Enables/disables timer.
	virtual void EnableTimer(const bool bEnable) = 0;

	//! \return True if timer is enabled
	virtual bool IsTimerEnabled() const = 0;

	//! Returns the current framerate in frames/second.
	virtual float GetFrameRate() = 0;

	//! Returns the fraction to blend current frame in profiling stats.
	virtual float GetProfileFrameBlending(float* pfBlendTime = 0, int* piBlendMode = 0) = 0;

	//! Serialization.
	virtual void Serialize(TSerialize ser) = 0;

	//! Tries to pause/unpause a timer.
	//! \return true if successfully paused/unpaused, false otherwise.
	virtual bool PauseTimer(ETimer which, bool bPause) = 0;

	//! Determines if a timer is paused.
	//! \return true if paused, false otherwise.
	virtual bool IsTimerPaused(ETimer which) = 0;

	//! Tries to set a timer.
	//! \return true if successful, false otherwise.
	virtual bool SetTimer(ETimer which, float timeInSeconds) = 0;

	//! Makes a tm struct from a time_t in UTC
	//! Example: Like gmtime.
	virtual void SecondsToDateUTC(time_t time, struct tm& outDateUTC) = 0;

	//! Makes a UTC time from a tm.
	//! Example: Like timegm, but not available on all platforms.
	virtual time_t DateToSecondsUTC(struct tm& timePtr) = 0;

	// Summary:
	//	Force fixed frame time. Passing time <= 0 will disable fixed frame time
	virtual void SetFixedFrameTime(float time) {};

	//! Convert from ticks (CryGetTicks()) to seconds.
	virtual float TicksToSeconds(int64 ticks) const = 0;

	//! Get number of ticks per second.
	virtual int64 GetTicksPerSecond() = 0;

	//! Create a new timer of the same type.
	virtual ITimer* CreateNewTimer() = 0;
	// </interfuscator:shuffle>
};

//! \cond INTERNAL
//! This class is used for automatic profiling of a section of the code.
//! Creates an instance of this class, and upon exiting from the code section.
template<typename time>
class CITimerAutoProfiler
{
public:
	CITimerAutoProfiler(ITimer* pTimer, time& rTime) :
		m_pTimer(pTimer),
		m_rTime(rTime)
	{
		rTime -= pTimer->GetAsyncCurTime();
	}

	~CITimerAutoProfiler()
	{
		m_rTime += m_pTimer->GetAsyncCurTime();
	}

protected:
	ITimer* m_pTimer;
	time&   m_rTime;
};
//! \endcond

//! Include this string AUTO_PROFILE_SECTION(pITimer, g_fTimer) for the section of code where the profiler timer must be turned on and off.
//! The profiler timer is just some global or static float or double value that accumulates the time (in seconds) spent in the given block of code.
//! pITimer is a pointer to the ITimer interface, g_fTimer is the global accumulator.
#define AUTO_PROFILE_SECTION(pITimer, g_fTimer) CITimerAutoProfiler<double> __section_auto_profiler(pITimer, g_fTimer)

#endif //_ITIMER_H_
