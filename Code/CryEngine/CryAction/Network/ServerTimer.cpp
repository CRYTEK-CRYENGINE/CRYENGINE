// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ServerTimer.h"

CServerTimer CServerTimer::m_this;

CServerTimer::CServerTimer()
{
	m_remoteFrameStartTime = 1.0f;
	m_frameTime = 0.001f;
	m_replicationTime = 0;
}

void CServerTimer::ResetTimer()
{
	CRY_ASSERT(false);
}

void CServerTimer::UpdateOnFrameStart()
{
	CTimeValue lastTime = m_remoteFrameStartTime;

	if (gEnv->bServer)
		m_remoteFrameStartTime = gEnv->pTimer->GetFrameStartTime();
	else if (INetChannel* pChannel = CCryAction::GetCryAction()->GetClientChannel())
	{
		if (pChannel->IsTimeReady())
			m_remoteFrameStartTime = pChannel->GetRemoteTime();
	}

	m_frameTime = (m_remoteFrameStartTime - lastTime).GetSeconds();
	m_frameTime = CLAMP(m_frameTime, 0.001f, 0.25f);

	m_replicationTime += m_frameTime;
}

float CServerTimer::GetCurrTime(ETimer /* which */) const
{
	return GetFrameStartTime().GetSeconds();
}

const CTimeValue& CServerTimer::GetFrameStartTime(ETimer /* which */) const
{
	return m_remoteFrameStartTime;
}

CTimeValue CServerTimer::GetAsyncTime() const
{
	return gEnv->pTimer->GetAsyncTime();
}

float CServerTimer::GetAsyncCurTime()
{
	return gEnv->pTimer->GetAsyncCurTime();
}

float CServerTimer::GetReplicationTime() const
{
	return m_replicationTime;
}

float CServerTimer::GetFrameTime(ETimer /* which */) const
{
	return m_frameTime;
}

float CServerTimer::GetRealFrameTime() const
{
	return gEnv->pTimer->GetRealFrameTime();
}

float CServerTimer::GetTimeScale() const
{
	return 1; //m_time_scale;
}

float CServerTimer::GetTimeScale(uint32 channel) const
{
	return 1.0f;
}

void CServerTimer::SetTimeScale(float scale, uint32 channel)
{
}

void CServerTimer::ClearTimeScales()
{
}

void CServerTimer::EnableTimer(const bool bEnable)
{
	CRY_ASSERT(false);
}

bool CServerTimer::IsTimerEnabled() const
{
	return true;
}

float CServerTimer::GetFrameRate()
{
	return gEnv->pTimer->GetFrameRate();
}

void CServerTimer::Serialize(TSerialize ser)
{
	CRY_ASSERT(false);
}

bool CServerTimer::PauseTimer(ETimer /* which */, bool /*bPause*/)
{
	assert(false);
	return false;
}

bool CServerTimer::IsTimerPaused(ETimer /* which */)
{
	return false;
}

bool CServerTimer::SetTimer(ETimer which, float timeInSeconds)
{
	return false;
}

void CServerTimer::SecondsToDateUTC(time_t time, struct tm& outDateUTC)
{
	gEnv->pTimer->SecondsToDateUTC(time, outDateUTC);
}

time_t CServerTimer::DateToSecondsUTC(struct tm& timePtr)
{
	return gEnv->pTimer->DateToSecondsUTC(timePtr);
}

float CServerTimer::TicksToSeconds(int64 ticks) const
{
	return gEnv->pTimer->TicksToSeconds(ticks);
}

int64 CServerTimer::GetTicksPerSecond()
{
	return gEnv->pTimer->GetTicksPerSecond();
}
