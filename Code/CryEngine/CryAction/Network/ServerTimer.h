// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERVERTIMER_H__
#define __SERVERTIMER_H__

#pragma once

// PERSONAL CRYTEK: Why is this timer updated on CActionGame::Update() instead of when the other global system timer is updated?
// Probably why it was split off like this. But.....why not just merge the functionality a little instead of all this nonsense? >.>
// For now, assert(false) everywhere to prevent nonsensical calls that redirect to global timer anyway/dont-apply to server timer.
class CServerTimer : public ITimer
{
public:
	static ITimer* Get() { return &m_this; }

	// Interface ITimer ----------------------------------------------------------
		// Lifecycle
			 ITimer* CreateNewTimer()							{ return new CServerTimer(); }		//<<
			 void  ResetTimer()									{ CRY_ASSERT(false); }
			 void  Serialize(TSerialize ser)					{ CRY_ASSERT(false); }

			 void  UpdateOnFrameStart();																		//<<
			 bool	 SetTimer(ETimer which, const CTimeValue& timeInSeconds){ assert(false); return false; }

			 bool	 PauseSimulation(bool bPause)				{ assert(false); return false; }
			 bool	 IsTimerPaused(ETimer which)				{ assert(false); return false; }


			 void  EnableTimer(const bool bEnable)			{ CRY_ASSERT(false); }
			 bool  IsTimerEnabled() const						{ assert(false); return true; }

		// Time getters
			 CTimeValue GetFrameTime(bool ignorePause = false) const { return m_frameTime; } //<<
			 CTimeValue GetRealFrameTime() const { CRY_ASSERT(false); return bogusTime; }

			 const CTimeValue& GetFrameStartTime(ETimer which = ETIMER_GAME) const { return m_remoteFrameStartTime; } //<<
			 const CTimeValue& GetRealStartTime()    const { assert(false); return bogusTime; }
			 const CTimeValue& GetAverageFrameTime() const { assert(false); return bogusTime; }
			 const CTimeValue& GetReplicationTime()  const { return m_replicationTime; };						//<<
			 const CTimeValue  GetServerTime()		  const { assert(false); return bogusTime; }

			 CTimeValue	GetAsyncTime()		const { assert(false); return bogusTime; }
			 CTimeValue GetAsyncCurTime() const { assert(false); return bogusTime; }

		// Timescales
			 mpfloat     GetTimeScale() const										{ assert(false); return 1; }
			 mpfloat     GetTimeScale(uint32 channel) const						{ assert(false); return 1; }
			 void        ClearTimeScales()											{ assert(false); };
			 void        SetTimeScale(const mpfloat& scale, uint32 channel = 0)	{ assert(false); };

		// Other misc.
			 CTimeValue  TicksToTime(int64 ticks) const { assert(false); return bogusTime; }
			 int64		 GetTicksPerSecond()		  const { assert(false); return 0; }

			 rTime       GetFrameRate()					  { assert(false); return rTime(0); }
			 mpfloat     GetProfileFrameBlending(CTimeValue* pfBlendTime = 0, int* piBlendMode = 0) { assert(false); return 1; }

			 void			 SecondsToDateUTC(time_t time, struct tm& outDateUTC) { assert(false); }
			 time_t		 DateToSecondsUTC(struct tm& timePtr)						{ assert(false); return time_t(); }
	// ~ Interface ITimer---------------------------------------------------------------------

private:
	CServerTimer();

	CTimeValue		bogusTime;

	CTimeValue     m_remoteFrameStartTime;
	CTimeValue     m_frameTime;
	CTimeValue		m_replicationTime;

	static CServerTimer m_this;
};

#endif
