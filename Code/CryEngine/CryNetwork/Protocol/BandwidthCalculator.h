// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __BANDWIDTHCALCULATOR_H__
#define __BANDWIDTHCALCULATOR_H__

#pragma once

#include <queue>
#include <CrySystem/TimeValue.h>
#include "Config.h"
#include "PMTUDiscovery.h"
#include <CryNetwork/INetwork.h>
#include "Config.h"

#if NEW_BANDWIDTH_MANAGEMENT

	#define MAX_TIME_DRIFT 3000

template<int N>
class CTimeRegression
{
public:
	CTimeRegression() : m_count(0), m_cached(false) {}

	void AddSample(CTimeValue x, CTimeValue y)
	{
		if (m_count)
		{
			// If either x or y increase by more than MAX_TIME_DRIFT milliseconds without the other one also
			// increasing then we need to reset our cache otherwise the ServerTime calculations will be broken
			int previousIndex = (m_count - 1) % N;

			int64 previousX = m_x[previousIndex].GetMilliSecondsAsInt64();
			int64 previousY = m_y[previousIndex].GetMilliSecondsAsInt64();
			int64 newX = x.GetMilliSecondsAsInt64();
			int64 newY = y.GetMilliSecondsAsInt64();

			int64 diffX = newX - previousX;
			int64 diffY = newY - previousY;

			int64 diffInTimes = (diffX - diffY);
			diffInTimes = (diffInTimes >= 0 ? diffInTimes : -diffInTimes);

			if (diffInTimes > MAX_TIME_DRIFT)
			{
				NetWarning("CTimerRegression::AddSample() diffInTimes %" PRId64 " (diff local %" PRId64 ", diff remote %" PRId64 ") - time diverged too much; clearing samples", diffInTimes, diffX, diffY);
				m_count = 0;
			}
		}

		int s = m_count++ % N;
		m_x[s] = x;
		m_y[s] = y;
		m_cached = false;
	}

	bool Empty() const
	{
		return m_count == 0;
	}

	size_t Size() const
	{
		return m_count > N ? N : m_count;
	}

	size_t Capacity() const
	{
		return N;
	}

	class CResult
	{
	public:
		CResult() : m_zeroTime(0.0f), m_intercept(0.0f), m_slope(0.0f) {}
		CResult(CTimeValue zeroTime, CTimeValue intercept, float slope) : m_zeroTime(zeroTime), m_intercept(intercept), m_slope(slope) {}

		CTimeValue RemoteTimeAt(CTimeValue localTime) const
		{
			return m_intercept + (localTime - m_zeroTime).GetSeconds() * m_slope;
		}

		CTimeValue GetZeroTime() const
		{
			return m_zeroTime;
		}

		float GetSlope() const
		{
			return m_slope;
		}

	private:
		CTimeValue m_zeroTime;
		CTimeValue m_intercept;
		float      m_slope;
	};

	bool GetRegression(CTimeValue zeroTime, CResult& result) const
	{
		bool haveResult = false;

		if (m_cached)
		{
			if (fabsf((zeroTime - m_cache.GetZeroTime()).GetSeconds()) <= 1.0f)
			{
				// have valid, cached result
				//NetLog("CTimerRegression::GetRegression() thinks we have cached value");
				result = m_cache;
				haveResult = true;
			}
			else
			{
				m_cached = false;
			}
		}

		if (!haveResult)
		{
			// Recalculate cache
			STATIC_CHECK(N > 3, NotEnoughElements);

			int sz = Size();
			//NetLog("CTimerRegression::GetRegression() thinks we have to recalculate cache (%d elements)", sz);
			if (sz >= 3)
			{
				CTimeValue averageX = 0.0f;
				CTimeValue averageY = 0.0f;

				for (int i = 0; i < sz; i++)
				{
					averageX += (m_x[i] - zeroTime);
					averageY += m_y[i];
				}
				averageX /= sz;
				averageY /= sz;

				float slopeNumer = 0.0f;
				float slopeDenom = 0.0f;

				for (int i = 0; i < sz; i++)
				{
					float xfactor = ((m_x[i] - zeroTime) - averageX).GetSeconds();
					slopeNumer += xfactor * (m_y[i] - averageY).GetSeconds();
					slopeDenom += xfactor * xfactor;
				}

				//NetLog("CTimerRegression::GetRegression() avgX %f, avgY %f, slopeNum %f, slopeDenom %f", averageX, averageY, slopeNumer, slopeDenom);
				if (fabsf(slopeDenom) >= 1e-6)
				{
					float slope = slopeNumer / slopeDenom;
					CTimeValue intercept = averageY - slope * averageX.GetSeconds();
					//NetLog("CTimerRegression::GetRegression() slope %f, intercept %f", slope, intercept.GetSeconds());
					result = m_cache = CResult(zeroTime, intercept, slope);
					m_cached = haveResult = true;
				}
			}
		}

		return haveResult;
	}

private:
	CTimeValue      m_x[N];
	CTimeValue      m_y[N];
	int             m_count;

	mutable bool    m_cached;
	mutable CResult m_cache;
};

class CTimedCounter
{
public:
	CTimedCounter(CTimeValue nMaxAge) : m_nMaxAge(nMaxAge) {}
	void AddSample(CTimeValue sample)
	{
		if (sample > m_nMaxAge)
			while (!m_samples.Empty() && m_samples.Front() < sample - m_nMaxAge)
				m_samples.Pop();
		if (m_samples.Full())
			m_samples.Pop();

		if (!m_samples.Empty())
		{
			CTimeValue backSample = m_samples.Back();
			//NET_ASSERT(backSample <= sample);
			// disable assert for now, and just verify things are safe
			if (backSample > sample)
				return;
		}
		m_samples.Push(sample);
	}
	CTimeValue FirstSampleTime()
	{
		return m_samples.Empty() ? 0.0f : m_samples.Front();
	}
	uint32 Size()
	{
		return uint32(m_samples.Size());
	}
	bool  Empty() { return m_samples.Empty(); }
	float CountsPerSecond()
	{
		return CountsPerSecond(m_samples.Empty() ? 0.0f : m_samples.Back());
	}
	float CountsPerSecond(CTimeValue nTime)
	{
		CTimeValue nBegin = FirstSampleTime();
		NET_ASSERT(nTime >= nBegin);
		return float(Size()) / (nTime - nBegin + 1.0f * (nTime == nBegin)).GetSeconds();
	}
	float CountsPerSecond(CTimeValue nTime, CTimeValue from)
	{
		Q::SIterator iter = m_samples.End();
		do
		{
			--iter;
		}
		while (*iter > from && iter != m_samples.Begin());
		//		++iter;
		if (iter == m_samples.End())
			return 0.0f;
		// *iter, not from --> we go one event earlier than the 'from' timestamp
		return float(m_samples.End() - iter) / (nTime - *iter).GetSeconds();
	}
	CTimeValue GetLast()
	{
		NET_ASSERT(!Empty());
		return m_samples.Back();
	}

private:
	CTimeValue m_nMaxAge;
	typedef MiniQueue<CTimeValue, 128> Q;
	Q          m_samples;
};

// this class is responsible for determining when we should send a packet
class CPacketRateCalculator
{
	typedef CTimeRegression<16> TTimeRegression;

public:
	enum eIncomingOutgoing
	{
		eIO_Outgoing,
		eIO_Incoming
	};

	// bandwidth range in bytes/second
	// (between 9600 bits/second and 100 megabits/second)
	static const uint32 MaxBandwidth = 100 * 1024 * 1024 / 8;
	static const uint32 MinBandwidth = 9600 / 8;

	CPacketRateCalculator();
	void                                    SentPacket(CTimeValue nTime, uint32 nSeq, uint16 nSize);
	void                                    GotPacket(CTimeValue nTime, uint16 nSize);
	void                                    AckedPacket(CTimeValue nTime, uint32 nSeq, bool bAck);
	void                                    AddPingSample(CTimeValue nTime, CTimeValue nPing, CTimeValue nRemoteTime);
	float                                   GetPing(bool smoothed) const;
	bool                                    NeedMorePings() const { return m_ping.Size() < m_ping.Capacity() / 2; }
	float                                   GetBandwidthUsage(CTimeValue nTime, CPacketRateCalculator::eIncomingOutgoing direction);
	float                                   GetPacketLossPerSecond(CTimeValue nTime);
	float                                   GetPacketLossPerPacketSent(CTimeValue nTime);
	float                                   GetPacketRate(bool idle, CTimeValue nTime);
	void                                    SetPerformanceMetrics(const INetChannel::SPerformanceMetrics& metrics);
	const INetChannel::SPerformanceMetrics* GetPerformanceMetrics(void) const { return &m_metrics; }
	CTimeValue                              GetRemoteTime() const;
	bool                                    IsTimeReady() const
	{
		TTimeRegression::CResult r;
		return m_timeRegression.GetRegression(g_time, r);
	}

	CTimeValue  PingMeasureDelay()     { return CTimeValue(1.0f); }

	TPacketSize GetMaxPacketSize(void) { return CNetCVars::Get().MaxPacketSize; }
	TPacketSize GetIdealPacketSize(const CTimeValue time, bool idle, TPacketSize maxPacketSize);
	TPacketSize GetSparePacketSize(const CTimeValue time, TPacketSize idealPacketSize, TPacketSize maxPacketSize
	#if LOG_BANDWIDTH_SHAPING
	                               , bool isLocal, const char* name
	#endif // LOG_BANDWIDTH_SHAPING
	                               );
	void       CalculateCurrentBandwidth(const CTimeValue time, float& packetsPerSecond, float& bytesPerSecond);
	CTimeValue GetNextPacketTime(int age, bool idle);

	float      GetCurrentPacketRate(CTimeValue nTime)
	{
		return m_sentPackets.CountsPerSecond(nTime);
	}

	void UpdateLatencyLab(CTimeValue nTime);

	bool IsSufferingHighLatency(CTimeValue nTime) const;

	void GetMemoryStatistics(ICrySizer* pSizer, bool countingThis = false)
	{
		SIZER_COMPONENT_NAME(pSizer, "CPacketRateCalculator");

		if (countingThis)
			pSizer->Add(*this);
	}

private:
	float GetAvailableBandwidth() const;

	INetChannel::SPerformanceMetrics      m_metrics;

	CCyclicStatsBuffer<float, 16>         m_ping;
	CCyclicStatsBuffer<CTimeValue, 128>   m_bandwidthUsedTime[2];
	CCyclicStatsBuffer<uint32, 128>       m_bandwidthUsedAmount[2];
	CCyclicStatsBuffer<float, 256>        m_tfrcHist;
	float                                 m_lastSlowStartRate;
	bool                                  m_hadAnyLoss;
	bool                                  m_allowSlowStartIncrease;
	float                                 m_rttEstimate;
	CTimeValue                            m_lastThroughputCalcTime;

	CTimedCounter                         m_sentPackets;
	CTimedCounter                         m_lostPackets;

	TTimeRegression                       m_timeRegression;

	mutable CTimeValue                    m_remoteTimeUpdated;
	mutable CTimeValue                    m_remoteTimeEstimate;
	mutable CTimeValue                    m_lastRemoteTimeEstimate;
	mutable CCyclicStatsBuffer<float, 16> m_timeVelocityBuffer;

	// number of milliseconds per second to move our clock to
	// sync to a remote clock
	const CTimeValue           m_minAdvanceTimeRate;
	const CTimeValue           m_maxAdvanceTimeRate;

	CTimeValue                 m_highLatencyStartTime;
	//std::map<uint32, CTimeValue> m_latencyLab;
	MiniQueue<CTimeValue, 127> m_latencyLab; // NOTE: the max value of N must NOT be bigger than 127 which is half the uint8 space (-128 ~ 127)!!!
	uint32                     m_latencyLabHighestSequence;
};

#endif // NEW_BANDWIDTH_MANAGEMENT

#endif // __BANDWIDTHCALCULATOR_H__
