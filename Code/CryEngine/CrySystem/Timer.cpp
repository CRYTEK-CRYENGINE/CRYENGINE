// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Timer.h"
#include <time.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ILog.h>
#include <CryNetwork/ISerialize.h>
// PERSONAL IMPROVE: the macro def location should be changed to avoid mess.
// For GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME setup.
#include "../CryAction/CryAction.h"
#include <CryNetwork\NetHelpers.h>
#include "../CryAction/Network/GameClientChannel.h"
#include "../CryAction/Network/GameClientNub.h"
/////////////////////////////////////////////////////
#include <CryCore/Platform/CryWindows.h>

#if CRY_PLATFORM_WINDOWS
	#include <Mmsystem.h> // needs <windows.h>
#endif

//#define PROFILING 1
#ifdef PROFILING
static int64 g_lCurrentTime = 0; // In ticks
#endif

/////////////////////////////////////////////////////
CTimer::CTimer()
{
	// Persistant state
	m_bEnabled = true;
	m_nFrameCounter = 0;

	LARGE_INTEGER TTicksPerSec;
	if (QueryPerformanceFrequency(&TTicksPerSec)) {
		// Performance counter is available, use it instead of multimedia timer
		m_lTicksPerSec = TTicksPerSec.QuadPart;
	} else {
		assert(false && "QueryPerformanceFrequency failed");
		//m_lTicksPerSec = 1000000;
	}

	m_frameTimes.resize(200);
	for (uint32 i = 0; i < 200; i++)
		m_frameTimes[i].SetSeconds("0.014");

	m_fAvgFrameTime.SetSeconds(0);
	m_fSmoothTime.SetSeconds(0);
	m_fProfileBlend = 1;

	m_totalTimeScale = 1;
	ClearTimeScales();

	ResetTimer();
}

bool CTimer::Init()
{
	REGISTER_CVAR2("t_MaxStep", &m_max_time_step, CTimeValue("0.25"), 0,
	               "Game systems clamped to this frame time. [default] = 0.25");

	// TODO: reconsider exposing this as cvar (negative time, same value is used by Trackview, better would be another value multipled with the internal one)
	REGISTER_CVAR2("t_Scale", &m_cvar_time_scale, 1, VF_NET_SYNCED | VF_DEV_ONLY,
		"Game (simulation) time scaled by this - for variable slow motion. [default] = 1");

	REGISTER_CVAR2("t_Smoothing", &m_TimeSmoothing, 1, 0,
		"Use averaged simulation frame time?"
		"0 = off, 1 = on [default]");

	REGISTER_CVAR2("average_interval", &average_interval, CTimeValue("0.25"), 0,
		"Interval over which average-frame-time is calculated. [default] = 0.25 seconds");


	/*------------== DEBUG ONLY ==------------*/
	REGISTER_CVAR2("t_Debug", &m_TimeDebug, 0, 0, "Timer debug: 0 = off [default], 1 = events, 2 = verbose");

	// Original default was .8 / log(10) ~= .35 seconds
	REGISTER_CVAR2("profile_smooth", &m_profile_smooth_time, 1, 0,
	               "Profiler exponential smoothing interval. [default] = 1 second");

	REGISTER_CVAR2("profile_weighting", &m_profile_weighting, 1, 0,
	               "Profiler smoothing mode: 0 = legacy, 1 = average [default], 2 = peak weighted, 3 = peak hold");

	REGISTER_CVAR2("t_FixedStep", &m_fixed_time_step, CTimeValue(0), VF_NET_SYNCED | VF_DEV_ONLY,
	               "Game simulation time pretends this was real-time, before scaling/smoothing/etc.\n"
	               "0 = off [default]\n"
	               "e.g. 0.033333(30 fps), 0.1(10 fps), 0.01(100 fps)");
	return true;
}

ITimer* CTimer::CreateNewTimer()
{
	return new CTimer();
}

void CTimer::ResetTimer()
{
	m_lBaseTime = CryGetTicks();
	m_lLastTime = 0;
	m_lOffsetTime = 0;

	m_fFrameTime.SetSeconds(0);
	m_fRealFrameTime.SetSeconds(0);
	m_replicationTime.SetSeconds(0);

	RefreshGameTime(0);
	RefreshUITime(0);

	m_bGameTimerPaused = false;
	m_lGameTimerPausedTime = 0;
}

void CTimer::Serialize(TSerialize ser)
{
	// Can't change m_lBaseTime, as this is used for async time (which shouldn't be affected by save games)
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
		const mpfloat multiplier = (mpfloat)m_lTicksPerSec / ticksPerSecond;
		curTime = (int64)(curTime * multiplier);

		SetOffsetToMatchGameTime(curTime);

		if (m_TimeDebug)
		{
			const int64 now = CryGetTicks();
			CryLogAlways("[CTimer]: Serialize: Frame=%d Last=%lld Now=%lld Off=%lld Async=%s GameTime=%s UI=%s", gEnv->pRenderer->GetFrameID(false), (long long)m_lLastTime, (long long)now, (long long)m_lOffsetTime, GetAsyncCurTime().str(), GetFrameStartTime(ETIMER_GAME).str(), GetFrameStartTime(ETIMER_UI).str());
		}
	}
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
		QueryPerformanceFrequency(&TTicksPerSec)
		m_lTicksPerSec = TTicksPerSec.QuadPart;
	}

	m_nFrameCounter++;
#endif

#ifdef PROFILING
	m_fRealFrameTime = m_fFrameTime = CTimeValue("0.020"); // 20ms = 50fps
	g_lCurrentTime += int64(m_fFrameTime.GetSeconds() * m_lTicksPerSec);
	m_lLastTime = g_lCurrentTime;
	RefreshGameTime(m_lLastTime);
	RefreshUITime(m_lLastTime);
	return;
#endif

	const int64 now = CryGetTicks();
	assert(now + 1 >= m_lBaseTime && "Invalid base time"); //+1 margin because QPC may be one off across cores

	// Get and set real frame time.
	int64 realTicks = now - m_lBaseTime - m_lLastTime;
	m_fRealFrameTime = TicksToTime(realTicks);

	// PERSONAL CRYTEK: What is the point of clamping? And why do this BEFORE time scaling in default Timer.cpp ?
	// Seriously need a discussion on timer related stuff.
	// Clamping should not be done on a by-system basis, needs to be centralized.
	// Physics is especially an offender there.
		// Ideally frameTime would have NO 'minimum' frame-time, and clamping should ONLY happen in CTimer, perhaps stored static macro's/values.
		// Or, at least, be trackable through CTimer rather than 'somewhere in the code somehow durh'

	// Simulation time = clamp'd & scaled real time.
	// If defined, use fixed-time-step as a base instead of real time.
	CTimeValue tmp = (m_fixed_time_step != 0 ? m_fixed_time_step : m_fRealFrameTime);
	m_fFrameTime = min(tmp * GetTimeScale(), m_max_time_step);

	// Update the running simulation frame-time average
	UpdateAverageFrameTime();

	// Use average'd frame-time, if enabled.
	if (m_TimeSmoothing > 0) {
		m_fFrameTime = GetAverageFrameTime();
	}

	/* PERSONAL NOTE: m_lBaseTime adjustment is a bad idea.
		'Rounding' errors and the like, hiding time-smoothing & scaling etc.
		This 'check' below doesn't make sense using time-scaling etc. unless m_lBaseTime is adjusted weirdly.
		assert(abs((TicksToTime(currentTime - m_lLastTime) - m_fFrameTime).GetSeconds()) < mpfloat("0.01") && "Bad calculation");


		// Adjust the base time so that time actually seems to have moved forward m_fFrameTime
		m_lBaseTime += realTicks - simTicks;
		if (m_lBaseTime > now)
		{
			// Guard against rounding errors due to float <-> int64 precision
			assert(m_lBaseTime - now <= 10 && "Bad base time or adjustment, too much difference for a rounding error");
			m_lBaseTime = now;
		}
	*/

	// In ticks
	const int64 currentTime = now - m_lBaseTime;

	assert(m_fRealFrameTime >= 0);
	assert(currentTime >= m_lLastTime && "Bad adjustment in previous frame");
	assert(currentTime + m_lOffsetTime >= 0 && "Sum of game time is negative");

	// Update all the timers
	RefreshUITime(currentTime);
	if (!m_bGameTimerPaused)
	{
		// PERSONAL CRYTEK: Running Game simulation time is NOT affected by time-scaling, smoothing, and clamping. Can only be paused/set()
		// This doesn't sound right, simulation time should follow simulation frame-time. If there are any errors they should be consistent.
		// Should make more sense once the purpose of smoothing/clamping is known. Time-scaling sounds like it should apply to simulation time for sure though.
		RefreshGameTime(currentTime);

		// PERSONAL CRYTEK: Why does replication time (used by networking) use simulation time and not real time?
		// Especially since GameTime() is NOT cumulative 'simulation frame time', e.g. not affected by averaging/smoothing/etc.
		// But this is!!!!
		m_replicationTime += m_fFrameTime;
	}
	m_lLastTime = currentTime;

	// DEBUG: Update profile-blending
	UpdateBlending();

	if (m_TimeDebug > 1)
	{
		CryLogAlways("[CTimer]: Frame=%d Cur=%lld Now=%lld Off=%lld Async=%s CurrTime=%s UI=%s", gEnv->pRenderer->GetFrameID(false), (long long)currentTime, (long long)now, (long long)m_lOffsetTime, GetAsyncCurTime().str(), GetFrameStartTime(ETIMER_GAME).str(), GetFrameStartTime(ETIMER_UI).str());
	}
}

/////////////////////////////////////////////////////
bool CTimer::SetTimer(ETimer which, const CTimeValue& timeInSeconds)
{
	if (which != ETIMER_GAME)
		return false;

	SetOffsetToMatchGameTime( int64(timeInSeconds.GetSeconds() * m_lTicksPerSec) );
	return true;
}

void CTimer::SetOffsetToMatchGameTime(int64 ticks)
{
	const int64 previousOffset = m_lOffsetTime;
	const CTimeValue previousGameTime = GetFrameStartTime(ETIMER_GAME);

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
		CryLogAlways("[CTimer] SetOffset: Offset %lld -> %lld, GameTime %s -> %s", (long long)previousOffset, (long long)m_lOffsetTime, previousGameTime.str(), GetFrameStartTime(ETIMER_GAME).str());
	}
}

void CTimer::RefreshGameTime(int64 ticks)
{
	assert(ticks + m_lOffsetTime >= 0);
	m_CurrTime[ETIMER_GAME] = TicksToTime(ticks + m_lOffsetTime);
}

void CTimer::RefreshUITime(int64 ticks)
{
	assert(ticks >= 0);
	m_CurrTime[ETIMER_UI] = TicksToTime(ticks);
}

/////////////////////////////////////////////////////
bool CTimer::IsTimerPaused(ETimer which)
{
	if (which != ETIMER_GAME)
		return false;
	return m_bGameTimerPaused;
}

bool CTimer::PauseSimulation(bool bPause)
{
	if (m_bGameTimerPaused == bPause)
		return false;

	m_bGameTimerPaused = bPause;

	if (bPause)
	{
		m_lGameTimerPausedTime = m_lLastTime + m_lOffsetTime;
		if (m_TimeDebug)
		{
			CryLogAlways("[CTimer]: Pausing ON: Frame=%d Last=%lld Off=%lld Async=%s CurrTime=%s UI=%s", gEnv->pRenderer->GetFrameID(false), (long long)m_lLastTime, (long long)m_lOffsetTime, GetAsyncCurTime().str(), GetFrameStartTime(ETIMER_GAME).str(), GetFrameStartTime(ETIMER_UI).str());
		}
	}
	else
	{
		SetOffsetToMatchGameTime(m_lGameTimerPausedTime);
		m_lGameTimerPausedTime = 0;
		if (m_TimeDebug)
		{
			CryLogAlways("[CTimer]: Pausing OFF: Frame=%d Last=%lld Off=%lld Async=%s CurrTime=%s UI=%s", gEnv->pRenderer->GetFrameID(false), (long long)m_lLastTime, (long long)m_lOffsetTime, GetAsyncCurTime().str(), GetFrameStartTime(ETIMER_GAME).str(), GetFrameStartTime(ETIMER_UI).str());
		}
	}

	return true;
}

void CTimer::EnableTimer(const bool bEnable)
{
	m_bEnabled = bEnable;
}

bool CTimer::IsTimerEnabled() const
{
	return m_bEnabled;
}

/////////////////////////////////////////////////////
CTimeValue CTimer::GetFrameTime(bool ignorePause) const
{
	return m_bEnabled && (ignorePause || !m_bGameTimerPaused) ? m_fFrameTime : 0;
}

CTimeValue CTimer::GetRealFrameTime() const
{
	return m_bEnabled ? m_fRealFrameTime : 0;
}

CTimeValue CTimer::GetAsyncCurTime() const
{
	int64 llNow = CryGetTicks() - m_lBaseTime;
	return CTimeValue(TicksToTime(llNow));
}

CTimeValue CTimer::GetAsyncTime() const
{
	int64 llNow = CryGetTicks();
	return CTimeValue(TicksToTime(llNow));
}

#if defined(GAME_CHANNEL_SYNC_CLIENT_SERVER_TIME)
const CTimeValue CTimer::GetServerTime() const
{
	if (gEnv->bServer)
		return GetAsyncTime();

	if (CGameClientNub* pGameClientNub = static_cast<CGameClientNub*>(gEnv->pGameFramework->GetIGameClientNub()))
	{
		if (CGameClientChannel* pGameClientChannel = pGameClientNub->GetGameClientChannel())
		{
			return (GetAsyncTime() + pGameClientChannel->GetClock().GetServerTimeOffset());
		}
	}

	return CTimeValue(0);
}
#else
// PERSONAL CRYTEK: The time's used by the two versions do not match.
// Default is absolute real time at moment of call, but this one here is in game-simulation start-of-frame time.
const CTimeValue CTimer::GetServerTime() const
{
	if (gEnv->bServer)
		return GetFrameStartTime();

	const auto* chan = gEnv->pGameFramework->GetClientChannel();
	return chan ? chan->GetRemoteTime() : CTimeValue(0);
}
#endif

/////////////////////////////////////////////////////
mpfloat CTimer::GetTimeScale() const
{
	return m_cvar_time_scale * m_totalTimeScale;
}

mpfloat CTimer::GetTimeScale(uint32 channel) const
{
	assert(channel < NUM_TIME_SCALE_CHANNELS);
	if (channel >= NUM_TIME_SCALE_CHANNELS)
	{
		return GetTimeScale();
	}
	return m_cvar_time_scale * m_timeScaleChannels[channel];
}

void CTimer::SetTimeScale(const mpfloat& scale, uint32 channel /* = 0 */)
{
	assert(channel < NUM_TIME_SCALE_CHANNELS);
	if (channel >= NUM_TIME_SCALE_CHANNELS)
	{
		return;
	}

	const mpfloat currentScale = m_timeScaleChannels[channel];

	if (scale != currentScale)
	{
		// Update total time scale immediately
		m_totalTimeScale *= scale / currentScale;
	}

	m_timeScaleChannels[channel] = scale;
}

void CTimer::ClearTimeScales()
{
	for (int i = 0; i < NUM_TIME_SCALE_CHANNELS; ++i)
	{
		m_timeScaleChannels[i] = 1;
	}
	m_totalTimeScale = 1;
}

/////////////////////////////////////////////////////
rTime CTimer::GetFrameRate()
{
	// Use real frame time.
	if (m_fRealFrameTime > 0)
		return (1 / m_fRealFrameTime);

	return 0;
}

void CTimer::UpdateBlending()
{
	// Accumulate smoothing time up to specified max.
	CTimeValue fFrameTime = m_fRealFrameTime;
	m_fSmoothTime = min(m_fSmoothTime + fFrameTime, m_profile_smooth_time);

	if (m_fSmoothTime <= fFrameTime)
	{
		m_fAvgFrameTime = fFrameTime;
		m_fProfileBlend = 1;
		return;
	}

	if (m_profile_weighting <= 2)
	{
		// Update average frame time.
		if (m_fSmoothTime < m_fAvgFrameTime)
			m_fAvgFrameTime = m_fSmoothTime;
		m_fAvgFrameTime *= (m_fSmoothTime / (m_fSmoothTime - fFrameTime + m_fAvgFrameTime)).conv<mpfloat>();

		if (m_profile_weighting == 1)
		{
			// Weight all frames equally.
			m_fProfileBlend = (m_fAvgFrameTime / m_fSmoothTime).conv<mpfloat>();
		}
		else
		{
			// Weight frames by time.
			m_fProfileBlend = (fFrameTime / m_fSmoothTime).conv<mpfloat>();
		}
	}
	else
	{
		// Decay avg frame time, set as new peak.
		m_fAvgFrameTime *= (1 - fFrameTime / m_fSmoothTime).conv<mpfloat>();
		if (fFrameTime > m_fAvgFrameTime)
		{
			m_fAvgFrameTime = fFrameTime;
			m_fProfileBlend = 1;
		}
		else
			m_fProfileBlend = 0;
	}
}

mpfloat CTimer::GetProfileFrameBlending(CTimeValue* pfBlendTime, int* piBlendMode)
{
	if (piBlendMode)
		*piBlendMode = m_profile_weighting;
	if (pfBlendTime)
		*pfBlendTime = m_fSmoothTime;
	return m_fProfileBlend;
}

//------------------------------------------------------------------------
//--  Average frame-times to avoid stalls and peaks in framerate
//--  NOTE: Averaged over a time period in ms, not over frames.
//------------------------------------------------------------------------
/*
	PERSONAL NOTE: Converted over CharManager's GetAverageFrameTime() to here.
	This was missing timescaling on FrameAmount, different clamp setup etc.......
*/
void CTimer::UpdateAverageFrameTime()
{
	// Clamped simulation-time (Again).						PERSONAL CRYTEK: More clamping for some reason! Should it be 0.25 or 0.2???
	CTimeValue cTime = CLAMP(m_fFrameTime, 0, "0.2");

	// Don't smooth if we pause the game.
	if (m_bGameTimerPaused) {
		m_prevAvgFrameTime = cTime;
		return;
	}

	// Shift and add frameTime
	uint32 numFT = m_frameTimes.size();
	for (int32 i = (numFT - 2); i > -1; i--)
		m_frameTimes[i + 1] = m_frameTimes[i];
	m_frameTimes[0] = cTime;

	// Setup smooth over a # of frames ~= TimeInterval
	int FrameAmount = 1;
	if (m_prevAvgFrameTime != 0)
	{
		FrameAmount = int( (average_interval * GetTimeScale()) / m_prevAvgFrameTime  + nTime("0.5") );
		FrameAmount = CLAMP(FrameAmount, 1, numFT);
	}

	// Do the smoothing/averaging
	CTimeValue AverageFrameTime;
	for (uint32 i = 0; i < FrameAmount; i++)
		AverageFrameTime += m_frameTimes[i];
	AverageFrameTime /= FrameAmount;

	m_prevAvgFrameTime = AverageFrameTime;
}

/////////////////////////////////////////////////////
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