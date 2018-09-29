// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CrySystem/ITimer.h>

/*
	Implements all common timing routines

	// CRYTEK TODO: Review m_time usage in System.cpp / SystemRender.cpp
	//					 if it wants Game Time / UI Time or a new Render Time?
*/
class CTimer : public ITimer
{
public:
	// Constructor & Destructor
	CTimer();
	~CTimer() {};

	bool Init();

	// Interface ITimer ----------------------------------------------------------
		// Lifecycle
			 ITimer* CreateNewTimer();
			 void  ResetTimer();
			 void  Serialize(TSerialize ser);

			 void  UpdateOnFrameStart();
			 bool	 SetTimer(ETimer which, const CTimeValue& timeInSeconds);

			 bool	 PauseSimulation(bool bPause);
			 bool	 IsTimerPaused(ETimer which);

			 void  EnableTimer(const bool bEnable);
			 bool  IsTimerEnabled() const;

		// Time getters
			 CTimeValue GetFrameTime(bool ignorePause = false) const;
			 CTimeValue GetRealFrameTime() const;

			 const CTimeValue& GetFrameStartTime(ETimer which = ETIMER_GAME) const { return m_CurrTime[(int)which]; }
			 const CTimeValue& GetAverageFrameTime() const { return m_prevAvgFrameTime; }
			 const CTimeValue& GetReplicationTime()  const { return m_replicationTime; };
			 const CTimeValue GetServerTime() const;

			 CTimeValue	GetAsyncTime() const;
			 CTimeValue GetAsyncCurTime() const;

		// Timescales
			 mpfloat     GetTimeScale() const;
			 mpfloat     GetTimeScale(uint32 channel) const;
			 void        ClearTimeScales();
			 void        SetTimeScale(const mpfloat& scale, uint32 channel = 0);

		// Other misc.
			 CTimeValue  TicksToTime(int64 ticks) const { return CTimeValue(ticks / (mpfloat)m_lTicksPerSec); }
			 int64		 GetTicksPerSecond()      const { return m_lTicksPerSec; }

			 rTime       GetFrameRate();
			 mpfloat     GetProfileFrameBlending(CTimeValue* pfBlendTime = 0, int* piBlendMode = 0); // DEBUG ONLY

			 void			 SecondsToDateUTC(time_t time, struct tm& outDateUTC);
			 time_t		 DateToSecondsUTC(struct tm& timePtr);
	// ~ Interface ITimer---------------------------------------------------------------------

private: 
	// Update profile-frame-blend time (smooth time). DEBUG ONLY
	void  UpdateBlending();

	// Updates CTimeValue, param = real delta-time in ticks.
	void  RefreshGameTime(int64 ticks);
	void  RefreshUITime(int64 ticks);

	// Update simulation frame-time average
	void UpdateAverageFrameTime();

	// Updates the game-time offset to match the the specified time.
	// The argument is the new number of ticks since the last Reset().
	void SetOffsetToMatchGameTime(int64 ticks);

private:

	enum
	{
		NUM_TIME_SCALE_CHANNELS = 8,
	};

	//////////////////////////////////////////////////////////////////////////
	// Dynamic state, reset by ResetTimer()
	//////////////////////////////////////////////////////////////////////////
	CTimeValue m_CurrTime[ETIMER_LAST]; // Real (UI) and Simulation (Game) time since last Reset().
	
	int64      m_lBaseTime;   // Absolute real-time (in ticks) at last Reset().
	int64      m_lLastTime;   // Absolute real-time (in ticks) since last Reset(). This is the base for UI time.
	int64      m_lOffsetTime; // Additional ticks for Game time (relative to UI time).
	
	CTimeValue m_replicationTime; // Sum of all simulation frame times used in replication.
	CTimeValue m_fFrameTime;      // Simulation time since the last Update()
	CTimeValue m_fRealFrameTime;  // Real time since the last Update()

	bool       m_bGameTimerPaused;     // Set if the game is paused.
	int64      m_lGameTimerPausedTime; // The UI time (in ticks) when the game timer was paused. On un-pause, offset time will be adjusted to match.


	//////////////////////////////////////////////////////////////////////////
	// Persistant state, kept by ResetTimer()
	//////////////////////////////////////////////////////////////////////////
	bool         m_bEnabled;		//!< Timer enabled/disabled ATM not used!
	unsigned int m_nFrameCounter; //!< Frame counter, ATM only used for pre-Vista TicksPerSec updates.
	int64			 m_lTicksPerSec;  //!< CPU ticks per second. pre-vista updated (approx. every 127 frames) in case of rate changes.

	// Frame averaging
	CTimeValue m_prevAvgFrameTime;			//!< The simulation frame-time averaged over a time period. (e.g. 0.25 seconds)
	std::vector<CTimeValue> m_frameTimes;	//!< Unsmoothed simulation frame-time list. [0] = Most recent.

	// Time scales
	mpfloat m_timeScaleChannels[NUM_TIME_SCALE_CHANNELS];
	mpfloat m_totalTimeScale;


	//////////////////////////////////////////////////////////////////////////
	// Console vars, always have default value on secondary CTimer instances
	//////////////////////////////////////////////////////////////////////////
	int   m_TimeSmoothing;			// Console Variable, 0=off, otherwise on
	mpfloat m_cvar_time_scale;		// Simulation time scale, used for slow-mo time

	CTimeValue m_max_time_step;	// Simulation frame-time cap
	CTimeValue average_interval;  // Frame-averaging time period.


	/*------------== DEBUG ONLY ==------------*/
	int   m_TimeDebug;			 // Console Variable, 0=off, 1=events, 2=verbose

	// If positive, simulation frame time is overriden by this before smoothing/scaling/etc.
	// If negative, also has FPS cap'd to this timestep via sleep.
	CTimeValue m_fixed_time_step;

	// Profile time smoothing (Persistent state)
	mpfloat m_fProfileBlend;	 // Current blending amount for profile.
	CTimeValue m_fAvgFrameTime; // Used for blend weighting (UpdateBlending())
	CTimeValue m_fSmoothTime;   // Smoothing interval (up to m_profile_smooth_time).

	// Profile averaging help.
	int   m_profile_weighting;	 // Weighting mode (see RegisterVar desc).

	// Time to exponentially smooth profile results.
	CTimeValue m_profile_smooth_time;  
};
