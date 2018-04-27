// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Timer.h"
#include <time.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ILog.h>
#include <CryNetwork/ISerialize.h>
/////////////////////////////////////////////////////
#include <CryCore/Platform/CryWindows.h>

#if CRY_PLATFORM_WINDOWS
	#include <Mmsystem.h> // needs <windows.h>
#endif

//#define PROFILING 1
#ifdef PROFILING
static int64 g_lCurrentTime = 0;
#endif

//! Profile smoothing time in seconds (original default was .8 / log(10) ~= .35 s)
static const float fDEFAULT_PROFILE_SMOOTHING = 1.0f;

#define DEFAULT_FRAME_SMOOTHING 1

/////////////////////////////////////////////////////
CTimer::CTimer()
{
	// Default CVar values
	m_fixed_time_step = 0;
	m_max_time_step = 0.25f;
	m_cvar_time_scale = 1.0f;
	m_TimeSmoothing = DEFAULT_FRAME_SMOOTHING; // note: frame numbers (old version - commented out) are not used but is based on time
	m_TimeDebug = 0;

	m_profile_smooth_time = fDEFAULT_PROFILE_SMOOTHING;
	m_profile_weighting = 1;

	// Persistant state
	m_bEnabled = true;
	m_nFrameCounter = 0;

	LARGE_INTEGER TTicksPerSec;
	if (QueryPerformanceFrequency(&TTicksPerSec))
	{
		// performance counter is available, use it instead of multimedia timer
		m_lTicksPerSec = TTicksPerSec.QuadPart;
	}
	else
	{
		assert(false && "QueryPerformanceFrequency failed");
		m_lTicksPerSec = 1000000;
	}
	m_fSecsPerTick = 1.0 / m_lTicksPerSec;

	m_fAverageFrameTime = 1.0f / 30.0f;
	for (int i = 0; i < MAX_FRAME_AVERAGE; i++)
	{
		m_arrFrameTimes[i] = m_fAverageFrameTime;
	}

	m_fAvgFrameTime = 0.0f;
	m_fProfileBlend = 1.0f;
	m_fSmoothTime = 0;

	m_totalTimeScale = 1.0f;
	ClearTimeScales();

	ResetTimer();
}

/////////////////////////////////////////////////////
bool CTimer::Init()
{
	// MartinM: renamed cvars for consistency
	// if game code was accessing them by name there was something wrong anyway

	REGISTER_CVAR2("t_Smoothing", &m_TimeSmoothing, DEFAULT_FRAME_SMOOTHING, 0,
	               "time smoothing\n"
	               "0=off, 1=on");

	REGISTER_CVAR2("t_FixedStep", &m_fixed_time_step, 0, VF_NET_SYNCED | VF_DEV_ONLY,
	               "Game updated with this fixed frame time\n"
	               "0=off, number specifies the frame time in seconds\n"
	               "e.g. 0.033333(30 fps), 0.1(10 fps), 0.01(100 fps)");

	REGISTER_CVAR2("t_MaxStep", &m_max_time_step, 0.25f, 0,
	               "Game systems clamped to this frame time");

	// todo: reconsider exposing that as cvar (negative time, same value is used by Trackview, better would be another value multipled with the internal one)
	REGISTER_CVAR2("t_Scale", &m_cvar_time_scale, 1.0f, VF_NET_SYNCED | VF_DEV_ONLY,
	               "Game time scaled by this - for variable slow motion");

	REGISTER_CVAR2("t_Debug", &m_TimeDebug, 0, 0, "Timer debug: 0 = off, 1 = events, 2 = verbose");

	// -----------------

	REGISTER_CVAR2("profile_smooth", &m_profile_smooth_time, fDEFAULT_PROFILE_SMOOTHING, 0,
	               "Profiler exponential smoothing interval (seconds)");

	REGISTER_CVAR2("profile_weighting", &m_profile_weighting, 1, 0,
	               "Profiler smoothing mode: 0 = legacy, 1 = average, 2 = peak weighted, 3 = peak hold");

	return true;
}

/////////////////////////////////////////////////////
float CTimer::GetFrameTime(ETimer which) const
{
	return m_bEnabled && (which != ETIMER_GAME || !m_bGameTimerPaused) ? m_fFrameTime : 0.0f;
}

float CTimer::GetReplicationTime() const
{
	return m_replicationTime;
}

/////////////////////////////////////////////////////
float CTimer::GetCurrTime(ETimer which) const
{
	assert(which >= 0 && which < ETIMER_LAST && "Bad timer index");
	return m_CurrTime[which].GetSeconds();
}

/////////////////////////////////////////////////////
float CTimer::GetRealFrameTime() const
{
	return m_bEnabled ? m_fRealFrameTime : 0.0f;
}

/////////////////////////////////////////////////////
float CTimer::GetTimeScale() const
{
	return m_cvar_time_scale * m_totalTimeScale;
}

/////////////////////////////////////////////////////
float CTimer::GetTimeScale(uint32 channel) const
{
	assert(channel < NUM_TIME_SCALE_CHANNELS);
	if (channel >= NUM_TIME_SCALE_CHANNELS)
	{
		return GetTimeScale();
	}
	return m_cvar_time_scale * m_timeScaleChannels[channel];
}

/////////////////////////////////////////////////////
void CTimer::SetTimeScale(float scale, uint32 channel /* = 0 */)
{
	assert(channel < NUM_TIME_SCALE_CHANNELS);
	if (channel >= NUM_TIME_SCALE_CHANNELS)
	{
		return;
	}

	const float currentScale = m_timeScaleChannels[channel];

	if (scale != currentScale)
	{
		// Need to adjust previous frame times for time scale to have immediate effect
		const float adjustFactor = scale / currentScale;
		for (uint32 i = 0; i < MAX_FRAME_AVERAGE; ++i)
		{
			m_arrFrameTimes[i] *= adjustFactor;
		}

		// Update total time scale immediately
		m_totalTimeScale *= adjustFactor;
	}

	m_timeScaleChannels[channel] = scale;
}

/////////////////////////////////////////////////////
void CTimer::ClearTimeScales()
{
	if (m_totalTimeScale != 1.0f)
	{
		// Need to adjust previous frame times for time scale to have immediate effect
		const float adjustFactor = 1.0f / m_totalTimeScale;
		for (uint32 i = 0; i < MAX_FRAME_AVERAGE; ++i)
		{
			m_arrFrameTimes[i] *= adjustFactor;
		}
	}

	for (int i = 0; i < NUM_TIME_SCALE_CHANNELS; ++i)
	{
		m_timeScaleChannels[i] = 1.0f;
	}
	m_totalTimeScale = 1.0f;
}

/////////////////////////////////////////////////////
float CTimer::GetAsyncCurTime()
{
	int64 llNow = CryGetTicks() - m_lBaseTime;
	return TicksToSeconds(llNow);
}

/////////////////////////////////////////////////////
float CTimer::GetFrameRate()
{
	// Use real frame time.
	if (m_fRealFrameTime != 0.f)
		return 1.f / m_fRealFrameTime;
	return 0.f;
}

void CTimer::UpdateBlending()
{
	// Accumulate smoothing time up to specified max.
	float fFrameTime = m_fRealFrameTime;
	m_fSmoothTime = min(m_fSmoothTime + fFrameTime, m_profile_smooth_time);

	if (m_fSmoothTime <= fFrameTime)
	{
		m_fAvgFrameTime = fFrameTime;
		m_fProfileBlend = 1.f;
		return;
	}

	if (m_profile_weighting <= 2)
	{
		// Update average frame time.
		if (m_fSmoothTime < m_fAvgFrameTime)
			m_fAvgFrameTime = m_fSmoothTime;
		m_fAvgFrameTime *= m_fSmoothTime / (m_fSmoothTime - fFrameTime + m_fAvgFrameTime);

		if (m_profile_weighting == 1)
		{
			// Weight all frames equally.
			m_fProfileBlend = m_fAvgFrameTime / m_fSmoothTime;
		}
		else
		{
			// Weight frames by time.
			m_fProfileBlend = fFrameTime / m_fSmoothTime;
		}
	}
	else
	{
		// Decay avg frame time, set as new peak.
		m_fAvgFrameTime *= 1.f - fFrameTime / m_fSmoothTime;
		if (fFrameTime > m_fAvgFrameTime)
		{
			m_fAvgFrameTime = fFrameTime;
			m_fProfileBlend = 1.f;
		}
		else
			m_fProfileBlend = 0.f;
	}
}

float CTimer::GetProfileFrameBlending(float* pfBlendTime, int* piBlendMode)
{
	if (piBlendMode)
		*piBlendMode = m_profile_weighting;
	if (pfBlendTime)
		*pfBlendTime = m_fSmoothTime;
	return m_fProfileBlend;
}

/////////////////////////////////////////////////////
void CTimer::RefreshGameTime(int64 curTime)
{
	assert(curTime + m_lOffsetTime >= 0);
	m_CurrTime[ETIMER_GAME].SetSeconds(TicksToSeconds(curTime + m_lOffsetTime));
}

/////////////////////////////////////////////////////
void CTimer::RefreshUITime(int64 curTime)
{
	assert(curTime >= 0);
	m_CurrTime[ETIMER_UI].SetSeconds(TicksToSeconds(curTime));
}

/////////////////////////////////////////////////////
void CTimer::UpdateOnFrameStart()
{
	if (!m_bEnabled)
		return;

	// On Windows before Vista, frequency can change (even though it should be impossible),
	// See also: https://msdn.microsoft.com/en-us/library/windows/desktop/dn553408(v=vs.85).aspx
	// Win2000, WinXP: Uses RDTSC, which may not be monotonic across all cores (a bug), costs in the order of 10~100 cycles (cheap).
	// WinVista: Uses HPET or ACPI timer (a kernel call, and much more expensive than RDTSC, but it's not bugged).
	// Win7+: RDTSC if the CPU feature bit for monotonic is set, HPET or ACPI otherwise (not bugged).
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600
	if ((m_nFrameCounter & 127) == 0)
	{
		// every bunch of frames, check frequency to adapt to
		// CPU power management clock rate changes
		LARGE_INTEGER TTicksPerSec;
		if (QueryPerformanceFrequency(&TTicksPerSec))
		{
			// if returns false, no performance counter is available
			m_lTicksPerSec = TTicksPerSec.QuadPart;
			m_fSecsPerTick = 1.0 / m_lTicksPerSec;
		}
	}

	m_nFrameCounter++;
#endif

#ifdef PROFILING
	m_fRealFrameTime = m_fFrameTime = 0.020f; // 20ms = 50fps
	g_lCurrentTime += (int)(m_fFrameTime * (float)(CTimeValue::TIMEVALUE_PRECISION));
	m_lLastTime = g_lCurrentTime;
	RefreshGameTime(m_lLastTime);
	RefreshUITime(m_lLastTime);
	return;
#endif

	if (m_fixed_time_step < 0.0f)
	{
		// Enforce real framerate by sleeping.
		const int64 elapsedTicks = CryGetTicks() - m_lBaseTime - m_lLastTime;
		const int64 minTicks = SecondsToTicks(-m_fixed_time_step);
		if (elapsedTicks < minTicks)
		{
			const int64 ms = (minTicks - elapsedTicks) * 1000 / m_lTicksPerSec;
			CrySleep((unsigned int)ms);
		}
	}

	const int64 now = CryGetTicks();
	assert(now + 1 >= m_lBaseTime && "Invalid base time"); //+1 margin because QPC may be one off across cores

	m_fRealFrameTime = TicksToSeconds(now - m_lBaseTime - m_lLastTime);

	if (0.0f != m_fixed_time_step)
	{
		// Apply fixed_time_step
		m_fFrameTime = abs(m_fixed_time_step);
	}
	else
	{
		// Clamp to max_time_step
		m_fFrameTime = min(m_fRealFrameTime, m_max_time_step);
	}

	// Dilate time.
	m_fFrameTime *= GetTimeScale();

	if (m_TimeSmoothing > 0)
	{
		m_fFrameTime = GetAverageFrameTime();
	}

	// Time can only go forward.
	if (m_fFrameTime < 0.0f) m_fFrameTime = 0.0f;
	if (m_fRealFrameTime < 0.0f) m_fRealFrameTime = 0.0;

	if (m_bEnabled && !m_bGameTimerPaused)
		m_replicationTime += m_fFrameTime;

	// Adjust the base time so that time actually seems to have moved forward m_fFrameTime
	const int64 frameTicks = SecondsToTicks(m_fFrameTime);
	const int64 realTicks = SecondsToTicks(m_fRealFrameTime);
	m_lBaseTime += realTicks - frameTicks;
	if (m_lBaseTime > now)
	{
		// Guard against rounding errors due to float <-> int64 precision
		assert(m_lBaseTime - now <= 10 && "Bad base time or adjustment, too much difference for a rounding error");
		m_lBaseTime = now;
	}
	const int64 currentTime = now - m_lBaseTime;

	assert(fabsf(TicksToSeconds(currentTime - m_lLastTime) - m_fFrameTime) < 0.01f && "Bad calculation");
	assert(currentTime >= m_lLastTime && "Bad adjustment in previous frame");
	assert(currentTime + m_lOffsetTime >= 0 && "Sum of game time is negative");

	// Update timers
	RefreshUITime(currentTime);
	if (!m_bGameTimerPaused)
	{
		RefreshGameTime(currentTime);
	}

	m_lLastTime = currentTime;

	UpdateBlending();

	if (m_TimeDebug > 1)
	{
		CryLogAlways("[CTimer]: Frame=%d Cur=%lld Now=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)currentTime, (long long)now, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
	}
}

//------------------------------------------------------------------------
//--  average frame-times to avoid stalls and peaks in framerate
//--    note that is is time-base averaging and not frame-based
//------------------------------------------------------------------------
float CTimer::GetAverageFrameTime()
{
	f32 LastAverageFrameTime = m_fAverageFrameTime;
	f32 FrameTime = m_fFrameTime;

	uint32 numFT = MAX_FRAME_AVERAGE;
	for (int32 i = (numFT - 2); i > -1; i--)
		m_arrFrameTimes[i + 1] = m_arrFrameTimes[i];

	if (FrameTime > 0.4f) FrameTime = 0.4f;
	if (FrameTime < 0.0f) FrameTime = 0.0f;
	m_arrFrameTimes[0] = FrameTime;

	//get smoothed frame
	uint32 avrg_ftime = 1;
	if (LastAverageFrameTime)
	{
		avrg_ftime = uint32(0.25f / LastAverageFrameTime + 0.5f); //average the frame-times for a certain time-period (sec)
		if (avrg_ftime > numFT) avrg_ftime = numFT;
		if (avrg_ftime < 1) avrg_ftime = 1;
	}

	f32 AverageFrameTime = 0;
	for (uint32 i = 0; i < avrg_ftime; i++)
		AverageFrameTime += m_arrFrameTimes[i];
	AverageFrameTime /= avrg_ftime;

	//don't smooth if we pause the game
	if (FrameTime < 0.0001f)
		AverageFrameTime = FrameTime;

	m_fAverageFrameTime = AverageFrameTime;
	return AverageFrameTime;
}

/////////////////////////////////////////////////////
void CTimer::ResetTimer()
{
	m_lBaseTime = CryGetTicks();
	m_lLastTime = 0;
	m_lOffsetTime = 0;

	m_fFrameTime = 0.0f;
	m_fRealFrameTime = 0.0f;
	m_replicationTime = 0;
	
	RefreshGameTime(0);
	RefreshUITime(0);

	m_bGameTimerPaused = false;
	m_lGameTimerPausedTime = 0;
}

/////////////////////////////////////////////////////
void CTimer::EnableTimer(const bool bEnable)
{
	m_bEnabled = bEnable;
}

bool CTimer::IsTimerEnabled() const
{
	return m_bEnabled;
}

/////////////////////////////////////////////////////
CTimeValue CTimer::GetAsyncTime() const
{
	int64 llNow = CryGetTicks();
	double fConvert = CTimeValue::TIMEVALUE_PRECISION * m_fSecsPerTick;
	return CTimeValue(int64(llNow * fConvert));
}

/////////////////////////////////////////////////////
void CTimer::Serialize(TSerialize ser)
{
	// cannot change m_lBaseTime, as this is used for async time (which shouldn't be affected by save games)
	if (ser.IsWriting())
	{
		int64 currentGameTime = m_lLastTime + m_lOffsetTime;

		ser.Value("curTime", currentGameTime);
		ser.Value("ticksPerSecond", m_lTicksPerSec);
	}
	else
	{
		int64 ticksPerSecond = 1, curTime = 1;
		ser.Value("curTime", curTime);
		ser.Value("ticksPerSecond", ticksPerSecond);

		// Adjust curTime for ticksPerSecond on this machine.
		// Some precision will be lost if the frequencies are not identical.
		const double multiplier = (double)m_lTicksPerSec / (double)ticksPerSecond;
		curTime = (int64)((double)curTime * multiplier);

		SetOffsetToMatchGameTime(curTime);

		if (m_TimeDebug)
		{
			const int64 now = CryGetTicks();
			CryLogAlways("[CTimer]: Serialize: Frame=%d Last=%lld Now=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)m_lLastTime, (long long)now, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
		}
	}
}

//! try to pause/unpause a timer
//  returns true if successfully paused/unpaused, false otherwise
bool CTimer::PauseTimer(ETimer which, bool bPause)
{
	if (which != ETIMER_GAME)
		return false;

	if (m_bGameTimerPaused == bPause)
		return false;

	m_bGameTimerPaused = bPause;

	if (bPause)
	{
		m_lGameTimerPausedTime = m_lLastTime + m_lOffsetTime;
		if (m_TimeDebug)
		{
			CryLogAlways("[CTimer]: Pausing ON: Frame=%d Last=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)m_lLastTime, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
		}
	}
	else
	{
		SetOffsetToMatchGameTime(m_lGameTimerPausedTime);
		m_lGameTimerPausedTime = 0;
		if (m_TimeDebug)
		{
			CryLogAlways("[CTimer]: Pausing OFF: Frame=%d Last=%lld Off=%lld Async=%f CurrTime=%f UI=%f", gEnv->pRenderer->GetFrameID(false), (long long)m_lLastTime, (long long)m_lOffsetTime, GetAsyncCurTime(), GetCurrTime(ETIMER_GAME), GetCurrTime(ETIMER_UI));
		}
	}

	return true;
}

//! determine if a timer is paused
//  returns true if paused, false otherwise
bool CTimer::IsTimerPaused(ETimer which)
{
	if (which != ETIMER_GAME)
		return false;
	return m_bGameTimerPaused;
}

//! try to set a timer
//  return true if successful, false otherwise
bool CTimer::SetTimer(ETimer which, float timeInSeconds)
{
	if (which != ETIMER_GAME)
		return false;

	SetOffsetToMatchGameTime(SecondsToTicks(timeInSeconds));
	return true;
}

ITimer* CTimer::CreateNewTimer()
{
	return new CTimer();
}

void CTimer::SecondsToDateUTC(time_t inTime, struct tm& outDateUTC)
{
	outDateUTC = *gmtime(&inTime);
}

#if CRY_PLATFORM_WINDOWS
time_t gmt_to_local_win32(void)
{
	TIME_ZONE_INFORMATION tzinfo;
	DWORD dwStandardDaylight;
	long bias;

	dwStandardDaylight = GetTimeZoneInformation(&tzinfo);
	bias = tzinfo.Bias;

	if (dwStandardDaylight == TIME_ZONE_ID_STANDARD)
		bias += tzinfo.StandardBias;

	if (dwStandardDaylight == TIME_ZONE_ID_DAYLIGHT)
		bias += tzinfo.DaylightBias;

	return (-bias * 60);
}
#endif

time_t CTimer::DateToSecondsUTC(struct tm& inDate)
{
#if CRY_PLATFORM_WINDOWS
	return mktime(&inDate) + gmt_to_local_win32();
#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#if defined(HAVE_TIMEGM)
	// return timegm(&inDate);
	#else
	// craig: temp disabled the +tm.tm_gmtoff because i can't see the intention here
	// and it doesn't compile anymore
	// alexl: tm_gmtoff is the offset to greenwhich mean time, whereas mktime uses localtime
	//        but not all linux distributions have it...
	return mktime(&inDate) /*+ tm.tm_gmtoff*/;
	#endif
#else
	return mktime(&inDate);
#endif
}

void CTimer::SetOffsetToMatchGameTime(int64 ticks)
{
	const int64 previousOffset = m_lOffsetTime;
	const float previousGameTime = GetCurrTime(ETIMER_GAME);

	m_lOffsetTime = ticks - m_lLastTime;
	RefreshGameTime(m_lLastTime);

	if (m_bGameTimerPaused)
	{
		// On un-pause, we will restore the specified time.
		// If we don't do this, the un-pause will over-write the offset again.
		m_lGameTimerPausedTime = ticks;
	}

	if (m_TimeDebug)
	{
		CryLogAlways("[CTimer] SetOffset: Offset %lld -> %lld, GameTime %f -> %f", (long long)previousOffset, (long long)m_lOffsetTime, GetCurrTime(ETIMER_GAME), previousGameTime);
	}
}

int64 CTimer::SecondsToTicks(double seconds) const
{
	return (int64)(seconds * (double)m_lTicksPerSec);
}
