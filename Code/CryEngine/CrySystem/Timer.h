// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CrySystem/ITimer.h>

// Implements all common timing routines
class CTimer : public ITimer
{
public:
	// constructor
	CTimer();
	// destructor
	~CTimer() {};

	bool Init();

	// interface ITimer ----------------------------------------------------------

	// TODO: Review m_time usage in System.cpp / SystemRender.cpp
	//       if it wants Game Time / UI Time or a new Render Time?

	virtual void       ResetTimer();
	virtual void       UpdateOnFrameStart();
	virtual float      GetCurrTime(ETimer which = ETIMER_GAME) const;
	virtual CTimeValue GetAsyncTime() const;
	virtual float      GetReplicationTime() const;	
	virtual float      GetAsyncCurTime();
	virtual float      GetFrameTime(ETimer which = ETIMER_GAME) const;
	virtual float      GetRealFrameTime() const;
	virtual float      GetTimeScale() const;
	virtual float      GetTimeScale(uint32 channel) const;
	virtual void       SetTimeScale(float scale, uint32 channel = 0);
	virtual void       ClearTimeScales();
	virtual void       EnableTimer(const bool bEnable);
	virtual float      GetFrameRate();
	virtual float      GetProfileFrameBlending(float* pfBlendTime = 0, int* piBlendMode = 0);
	virtual void       Serialize(TSerialize ser);
	virtual bool       IsTimerEnabled() const;

	//! try to pause/unpause a timer
	//  returns true if successfully paused/unpaused, false otherwise
	virtual bool PauseTimer(ETimer which, bool bPause);

	//! determine if a timer is paused
	//  returns true if paused, false otherwise
	virtual bool IsTimerPaused(ETimer which);

	//! try to set a timer
	//  return true if successful, false otherwise
	virtual bool SetTimer(ETimer which, float timeInSeconds);

	//! make a tm struct from a time_t in UTC (like gmtime)
	virtual void SecondsToDateUTC(time_t time, struct tm& outDateUTC);

	//! make a UTC time from a tm (like timegm, but not available on all platforms)
	virtual time_t DateToSecondsUTC(struct tm& timePtr);

	//! Convert from Tics to Seconds
	virtual float TicksToSeconds(int64 ticks) const
	{
		return float((double)ticks * m_fSecsPerTick);
	}

	//! Get number of ticks per second
	virtual int64 GetTicksPerSecond()
	{
		return m_lTicksPerSec;
	}

	virtual void SetFixedFrameTime(float time)
	{
		m_fixed_time_step = time;
	}

	virtual const CTimeValue& GetFrameStartTime(ETimer which = ETIMER_GAME) const { return m_CurrTime[(int)which]; }
	virtual ITimer*           CreateNewTimer();

private: // ---------------------------------------------------------------------

	// ---------------------------------------------------------------------------

	// updates m_CurrTime (either pass m_lCurrentTime or custom curTime)
	void  RefreshGameTime(int64 curTime);
	void  RefreshUITime(int64 curTime);
	void  UpdateBlending();
	float GetAverageFrameTime();

	// Updates the game-time offset to match the the specified time.
	// The argument is the new number of ticks since the last Reset().
	void SetOffsetToMatchGameTime(int64 ticks);

	// Convert seconds to ticks using the timer frequency.
	// Note: Loss of precision may occur, especially if magnitude of argument or timer frequency is large.
	int64 SecondsToTicks(double seconds) const;

	enum
	{
		MAX_FRAME_AVERAGE       = 100,
		NUM_TIME_SCALE_CHANNELS = 8,
	};

	//////////////////////////////////////////////////////////////////////////
	// Dynamic state, reset by ResetTimer()
	//////////////////////////////////////////////////////////////////////////
	CTimeValue m_CurrTime[ETIMER_LAST]; // Time since last Reset(), cached during Update()

	int64      m_lBaseTime;   // Ticks elapsed since system boot, all other tick-unit variables are relative to this.
	int64      m_lLastTime;   // Ticks since last Reset(). This is the base for UI time. UI time is monotonic, it always moves forward at a constant rate until the timer is Reset()).
	int64      m_lOffsetTime; // Additional ticks for Game time (relative to UI time). Game time can be affected by loading, pausing, time smoothing and time clamping, as well as SetTimer().
	float      m_replicationTime; // In seconds, sum of all frame times used in replication

	float      m_fFrameTime;     // In seconds since the last Update(), clamped/smoothed etc.
	float      m_fRealFrameTime; // In real seconds since the last Update(), non-clamped/un-smoothed etc.

	bool       m_bGameTimerPaused;     // Set if the game is paused. GetFrameTime() will return 0, GetCurrTime(ETIMER_GAME) will not progress.
	int64      m_lGameTimerPausedTime; // The UI time when the game timer was paused. On un-pause, offset will be adjusted to match.

	//////////////////////////////////////////////////////////////////////////
	// Persistant state, kept by ResetTimer()
	//////////////////////////////////////////////////////////////////////////
	bool         m_bEnabled;
	unsigned int m_nFrameCounter;

	int64        m_lTicksPerSec; // Ticks per second
	double       m_fSecsPerTick; // Seconds per tick

	// smoothing
	float m_arrFrameTimes[MAX_FRAME_AVERAGE];
	float m_fAverageFrameTime; // used for smoothing (AverageFrameTime())

	float m_fAvgFrameTime;     // used for blend weighting (UpdateBlending())
	float m_fProfileBlend;     // current blending amount for profile.
	float m_fSmoothTime;       // smoothing interval (up to m_profile_smooth_time).

	// time scale
	float m_timeScaleChannels[NUM_TIME_SCALE_CHANNELS];
	float m_totalTimeScale;

	//////////////////////////////////////////////////////////////////////////
	// Console vars, always have default value on secondary CTimer instances
	//////////////////////////////////////////////////////////////////////////
	float m_fixed_time_step;   // in seconds
	float m_max_time_step;     // in seconds
	float m_cvar_time_scale;   // slow down time cvar
	int   m_TimeSmoothing;     // Console Variable, 0=off, otherwise on
	int   m_TimeDebug;         // Console Variable, 0=off, 1=events, 2=verbose

	// Profile averaging help.
	float m_profile_smooth_time;  // seconds to exponentially smooth profile results.
	int   m_profile_weighting;    // weighting mode (see RegisterVar desc).
};
