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

public: //** Lifecycle

	//! Create a new timer of the same type.
	virtual ITimer* CreateNewTimer() = 0;

	//! Resets the timer (ATM the only timer Reset() call is at system boot, CSystem::Initialize())
	//! \note Kept as a just-in-case, for out of memory constraints or tests, long running servers etc.
	virtual void ResetTimer() = 0;

	//! Serialization.
	virtual void Serialize(TSerialize ser) = 0;

	//! Updates the timer once per frame. ATM called during the start of System::Update() & CActionGame::BlockingConnect()
	virtual void UpdateOnFrameStart() = 0;

	//! Tries to set a timer.
	//! \return true if successful, false otherwise.
	virtual bool SetTimer(ETimer which, const CTimeValue& timeInSeconds) = 0;

	//! Pauses simulation time. GetFrameTime() will return 0, GetFrameStartTime(ETIMER_GAME) will not progress.
	//! \return true if successfully paused/unpaused, false otherwise.
	virtual bool PauseSimulation(bool bPause) = 0;

	//! Determines if a timer is paused.
	//! \return true if paused, false otherwise.
	virtual bool IsTimerPaused(ETimer which) = 0;

	//! Enables/disables timer. ATM not used anywhere.
	virtual void EnableTimer(const bool bEnable) = 0;

	//! \return True if timer is enabled
	virtual bool IsTimerEnabled() const = 0;


public: //** Time Getters
	/*
		2 types of times:
			Async or ServerTime == At moment of function call
			Everything else: Cached during UpdateOnFrameStart()
	*/

	//! Returns simulation time since the last UpdateOnFrameStart(), clamped/smoothed/scaled etc.
	//! \note Pass true to ignore pauses
	virtual CTimeValue GetFrameTime(bool ignorePause = false) const = 0;

	//! Returns real-time since the last UpdateOnFrameStart(), no dilation/smoothing/clamping etc
	virtual CTimeValue GetRealFrameTime() const = 0;

	// PERSONAL IMPROVE: The lack of a 'real start time' thats not relative to Reset() might be an issue, can easily be added in.
	// Resets are finicky.

	//! Returns Real (UI) or Simulation (Game) time since last timer Reset().
	//! UI time is monotonic, it always moves forward at a constant rate until the timer is Reset().
	//! Game time can be affected by loading, pausing, time smoothing, time dilation and time clamping, as well as SetTimer(). 
	//! UI time = real-start time
	//! PERSONAL CRYTEK: NOPE! See comments in timer.cpp on ACTUAL game-time, not just game-frame-time...
	virtual const CTimeValue& GetFrameStartTime(ETimer which = ETIMER_GAME) const = 0;

	//! Returns sum of simulation frame-time's. Used for networking.
	virtual const CTimeValue& GetReplicationTime() const = 0;

	//! Returns the (synched) absolute real-time of the server at moment of call. (So use this for timed events, such as MP round times)
	//! Not to be confused with CServerTimer
	virtual const CTimeValue GetServerTime() const = 0;

	//! Returns recent simulation frame-time's averaged over a time period (e.g. 0.25 seconds)
	virtual const CTimeValue& GetAverageFrameTime() const = 0;

	//! Returns absolute real-time at moment of call.
	virtual CTimeValue GetAsyncTime() const = 0;

	//! Returns elapsed time (at moment of call) since the last timer Reset()
	virtual CTimeValue GetAsyncCurTime() const = 0;


public: //** Timescales

	// PERSONAL CRYTEK: As noted elsewhere, only affects simulation-frame-time!!!! and also replication time.... 
	//! Returns the time scale applied on simulation time.
	virtual mpfloat GetTimeScale() const = 0;

	//! Returns the time scale factor for the given channel
	virtual mpfloat GetTimeScale(uint32 channel) const = 0;

	//! Clears all current time scales
	virtual void ClearTimeScales() = 0;

	//! Sets the time scale applied on simulation time.
	virtual void SetTimeScale(const mpfloat& s, uint32 channel = 0) = 0;


public: //** Other misc.

  //! Get number of CPU ticks per second.
	virtual int64 GetTicksPerSecond() const = 0;

	//! Convert from CPU ticks (e.g. CryGetTicks()) to CTimeValue
	virtual CTimeValue TicksToTime(int64 ticks) const = 0;

	//! Returns the current real framerate in frames/second.
	virtual rTime GetFrameRate() = 0;

	//! Returns the fraction to blend current frame in profiling stats. DEBUG ONLY
	virtual mpfloat GetProfileFrameBlending(CTimeValue* pfBlendTime = 0, int* piBlendMode = 0) = 0;

	//! Makes a tm struct from a time_t in UTC
	//! Example: Like gmtime.
	virtual void SecondsToDateUTC(time_t time, struct tm& outDateUTC) = 0;

	//! Makes a UTC time from a tm.
	//! Example: Like timegm, but not available on all platforms.
	virtual time_t DateToSecondsUTC(struct tm& timePtr) = 0;
	// </interfuscator:shuffle>
};

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

//! Include this string AUTO_PROFILE_SECTION(pITimer, g_fTimer) for the section of code where the profiler timer must be turned on and off.
//! The profiler timer is just some global CTimeValue that accumulates the time spent in the given block of code.
//! pITimer is a pointer to the ITimer interface, g_fTimer is the global accumulator.
#define AUTO_PROFILE_SECTION(pITimer, g_fTimer) CITimerAutoProfiler<CTimeValue> __section_auto_profiler(pITimer, g_fTimer)

#endif //_ITIMER_H_
