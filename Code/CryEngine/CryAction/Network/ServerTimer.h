// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __SERVERTIMER_H__
#define __SERVERTIMER_H__

#pragma once

class CServerTimer : public ITimer
{
public:
	virtual void              ResetTimer();
	virtual void              UpdateOnFrameStart();
	virtual float             GetCurrTime(ETimer which = ETIMER_GAME) const;
	virtual const CTimeValue& GetFrameStartTime(ETimer which = ETIMER_GAME) const;
	virtual CTimeValue        GetAsyncTime() const;
	virtual float             GetReplicationTime() const;
	virtual float             GetAsyncCurTime();
	virtual float             GetFrameTime(ETimer which = ETIMER_GAME) const;
	virtual float             GetRealFrameTime() const;
	virtual float             GetTimeScale() const;
	virtual float             GetTimeScale(uint32 channel) const;
	virtual void              SetTimeScale(float scale, uint32 channel = 0);
	virtual void              ClearTimeScales();
	virtual void              EnableTimer(const bool bEnable);
	virtual bool              IsTimerEnabled() const;
	virtual float             GetFrameRate();
	virtual float             GetProfileFrameBlending(float* pfBlendTime = 0, int* piBlendMode = 0) { return 1.f; }
	virtual void              Serialize(TSerialize ser);
	virtual bool              PauseTimer(ETimer which, bool bPause);
	virtual bool              IsTimerPaused(ETimer which);
	virtual bool              SetTimer(ETimer which, float timeInSeconds);
	virtual void              SecondsToDateUTC(time_t time, struct tm& outDateUTC);
	virtual time_t            DateToSecondsUTC(struct tm& timePtr);
	virtual float             TicksToSeconds(int64 ticks) const;
	virtual int64             GetTicksPerSecond();
	virtual ITimer*           CreateNewTimer() { return new CServerTimer(); }

	static ITimer*            Get()
	{
		return &m_this;
	}

private:
	CServerTimer();

	CTimeValue          m_remoteFrameStartTime;
	float               m_frameTime;
	float				m_replicationTime;

	static CServerTimer m_this;
};

#endif
