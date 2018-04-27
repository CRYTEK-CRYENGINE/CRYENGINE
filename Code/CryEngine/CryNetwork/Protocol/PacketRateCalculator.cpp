// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  calculates how long we should wait between packets, and how
              large those packets should be
   -------------------------------------------------------------------------
   History:
   - 26/07/2004   10:34 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "PacketRateCalculator.h"
#include "Network.h"
#include <CrySystem/IConsole.h>

#if !NEW_BANDWIDTH_MANAGEMENT

	#undef min
	#undef max

static float Clamp(float x, float mn, float mx)
{
	if (x < mn) return mn;
	if (x > mx) return mx;
	return x;
}

static void CalculateDesiredMinMax(ICVar* pDesired, ICVar* pTolLow, ICVar* pTolHigh,
                                   float& des, float& mn, float& mx)
{
	des = pDesired->GetFVal();
	if (des < 0.0f) des = 0.0f;
	float tolLow = pTolLow->GetFVal();
	if (tolLow < 0.0f) tolLow = 0.0f;
	else if (tolLow > 1.0f)
		tolLow = 1.0f;
	float tolHigh = pTolHigh->GetFVal();
	if (tolHigh < 0.0f) tolHigh = 0.0f;
	mn = des * (1.0f - tolLow);
	mx = des * (1.0f + tolHigh);
}

static void CalculateDesiredMinMax(float desired, float tolLow, float tolHigh, float& des, float& mn, float& mx)
{
	des = desired;
	if (des < 0.0f) des = 0.0f;
	if (tolLow < 0.0f) tolLow = 0.0f;
	else if (tolLow > 1.0f)
		tolLow = 1.0f;
	if (tolHigh < 0.0f) tolHigh = 0.0f;
	mn = des * (1.0f - tolLow);
	mx = des * (1.0f + tolHigh);
}

struct CPacketRateCalculator::SPacketSizerParams
{
	float datCur;
	float tCur;
	float bwDes;
	float bwMax;
	float bwMin;
	float prDes;
	float prMax;
	float prMin;
};

struct CPacketRateCalculator::SPacketSizerResult
{
	float dt;
	float ps;
};

CPacketRateCalculator::CPacketRateCalculator() :
	m_sentPackets(5.0f),
	m_lostPackets(5.0f),
	m_minAdvanceTimeRate(0.80f),
	m_maxAdvanceTimeRate(1.20f),
	m_lastSlowStartRate(0.0f),
	m_hadAnyLoss(false),
	m_allowSlowStartIncrease(false),
	m_latencyLabHighestSequence(0),
	m_remoteTimeEstimate(0.0f)
{
	m_lastThroughputCalcTime = 0.0f;
	m_rttEstimate = 1.0f;
	SetPerformanceMetrics(INetChannel::SPerformanceMetrics());
}

float CPacketRateCalculator::GetPing(bool smoothed) const
{
	if (m_ping.Empty())
		return 0.0f;
	else if (smoothed)
		return m_ping.GetMedian();
	else
		return m_ping.GetLast();
}

float CPacketRateCalculator::GetBandwidthUsage(CTimeValue nTime, CPacketRateCalculator::eIncomingOutgoing direction)
{
	NET_ASSERT(m_bandwidthUsedTime[direction].Size() == m_bandwidthUsedAmount[direction].Size());

	uint32 total = m_bandwidthUsedAmount[direction].GetTotal();
	float time = m_bandwidthUsedTime[direction].Empty() ? 1.0f :
	             (nTime - m_bandwidthUsedTime[direction].GetFirst()).GetSeconds();
	// ensure time >= 1
	if (time < 1.0f)
		time = 1.0f;

	//	NetLog("GetBandwidthUsage: %d in %d = %d", total, time, 1000*total/time);

	return (static_cast<float>(total) / time);
}

float CPacketRateCalculator::GetPacketLossPerSecond(CTimeValue nTime)
{
	return m_lostPackets.CountsPerSecond(nTime);
}

float CPacketRateCalculator::GetPacketLossPerPacketSent(CTimeValue nTime)
{
	if (m_sentPackets.Empty() || m_lostPackets.Empty())
		return 0.0f;

	CTimeValue firstTime = std::max(m_sentPackets.FirstSampleTime(), m_lostPackets.FirstSampleTime());

	float pktRate = m_sentPackets.CountsPerSecond(nTime, firstTime);
	float lossRate = m_lostPackets.CountsPerSecond(nTime, firstTime);
	return lossRate / pktRate;
}

float CPacketRateCalculator::GetPacketRate(bool idle, CTimeValue nTime)
{
	float des, mn, mx;
	CalculateDesiredMinMax(
	  idle ? m_metrics.pIdlePacketRateDesired : m_metrics.pPacketRateDesired,
	  m_metrics.pPacketRateToleranceLow,
	  m_metrics.pPacketRateToleranceHigh,
	  des, mn, mx
	  );
	return Clamp(des / std::max(0.01f, 1.0f - GetPacketLossPerPacketSent(nTime)), mn, mx);
}

void CPacketRateCalculator::SentPacket(CTimeValue nTime, uint32 nSeq, uint16 nSize)
{
	m_sentPackets.AddSample(nTime);
	m_bandwidthUsedTime[eIO_Outgoing].AddSample(nTime);
	m_bandwidthUsedAmount[eIO_Outgoing].AddSample(nSize + UDP_HEADER_SIZE);

	m_pmtuDiscovery.SentPacket(nTime, nSeq, nSize);

	// TODO: make sure nTime is the current time, otherwise we have to use gEnv->pTimer->GetAsyncTime()
	//m_latencyLab[nSeq] = nTime;
	m_latencyLab.CyclePush(nTime);
	m_latencyLabHighestSequence = nSeq;
	// the assumption is that sequence numbers are consequtive
}

void CPacketRateCalculator::GotPacket(CTimeValue nTime, uint16 nSize)
{
	m_bandwidthUsedTime[eIO_Incoming].AddSample(nTime);
	m_bandwidthUsedAmount[eIO_Incoming].AddSample(nSize + UDP_HEADER_SIZE);
}

void CPacketRateCalculator::AckedPacket(CTimeValue nTime, uint32 nSeq, bool bAck)
{
	if (!bAck)
	{
		m_hadAnyLoss = true;
		m_lostPackets.AddSample(nTime);
	}
	else
	{
		m_allowSlowStartIncrease = true;
	}

	m_pmtuDiscovery.AckedPacket(nTime, nSeq, bAck);

	// TODO: make sure nTime is the current time, otherwise we have to use gEnv->pTimer->GetAsyncTime()
	if (bAck && !m_latencyLab.Empty())
	{
		uint32 sz = m_latencyLab.Size() - 1;
		uint32 lowestSeq = m_latencyLabHighestSequence - sz;                 // two's complement - uint32
		int32 i = nSeq - lowestSeq, i1 = m_latencyLabHighestSequence - nSeq; // two's complement - uint32 => int32
		if (i >= 0 && i1 >= 0)                                               // in range sequence
		{
			CTimeValue latency = nTime - m_latencyLab[i];
			if (latency >= CVARS.HighLatencyThreshold)
			{
				if (m_highLatencyStartTime == 0.0f)
					m_highLatencyStartTime = nTime;
			}
			else
				m_highLatencyStartTime = 0.0f;
			while (true)
			{
				m_latencyLab.Pop();
				if (i <= 0)
					break;
				--i;
			}
		}
	}
}

void CPacketRateCalculator::UpdateLatencyLab(CTimeValue nTime)
{
	// TODO: make sure nTime is the current time, otherwise we need to use gEnv->pTimer->GetAsyncTime()
	MiniQueue<CTimeValue, 127>::SIterator itor = m_latencyLab.Begin();
	for (; itor != m_latencyLab.End(); ++itor)
	{
		CTimeValue latency = nTime - *itor;
		if (latency < CVARS.HighLatencyThreshold)
			break;

		if (m_highLatencyStartTime == 0.0f)
			m_highLatencyStartTime = nTime;
	}

	m_latencyLab.Erase(m_latencyLab.Begin(), itor);
}

bool CPacketRateCalculator::IsSufferingHighLatency(CTimeValue nTime) const
{
	if (CVARS.HighLatencyThreshold <= 0.0f)
		return false;

	if (m_highLatencyStartTime == 0.0f)
		return false;

	return nTime - m_highLatencyStartTime >= CVARS.HighLatencyTimeLimit;
}

CTimeValue CPacketRateCalculator::GetRemoteTime() const
{
	static const float MAX_STEP_SIZE = 0.01f;

	TTimeRegression::CResult r;

	float age = (g_time - m_remoteTimeUpdated).GetSeconds();
	if (fabsf(age) > 0.5f)
	{
		if (CVARS.RemoteTimeEstimationWarning != 0)
		{
			NetWarning("[time] remote time estimation restarted; estimate accumulator age was %f", age);
		}
		bool ok = m_timeRegression.GetRegression(g_time, r);
		NET_ASSERT(ok);
		if (!ok)
		{
			//			branch = 1;
			m_remoteTimeUpdated = 0.0f;
		}
		else
		{
			//			branch = 2;
			m_remoteTimeUpdated = g_time;
			m_remoteTimeEstimate = r.RemoteTimeAt(g_time);
			m_timeVelocityBuffer.Clear();
		}
		//		NetLog("[time] RESET: %f %f %f", m_remoteTimeUpdated.GetSeconds(), m_remoteTimeEstimate.GetSeconds(), m_remoteTimeVelocity);
	}
	else if (age > 0)
	{
		bool ok = m_timeRegression.GetRegression(g_time, r);
		NET_ASSERT(ok);
		if (!ok)
		{
			//			branch = 3;
			m_remoteTimeUpdated = 0.0f;
		}
		else
			do
			{
				//			branch = 4;
				float step = std::min(age, MAX_STEP_SIZE);
				const CTimeValue targetPos = r.RemoteTimeAt(m_remoteTimeUpdated);

				if (fabsf((m_remoteTimeEstimate - targetPos).GetSeconds()) < 1.25f)
				{
					const float targetVel = r.GetSlope();
					const float maxVel = 20.0f;
					const float minVel = 1.0f / maxVel;

					const float rawVelocity = Clamp(targetVel + (targetPos - m_remoteTimeEstimate).GetSeconds(), minVel, maxVel);
					m_timeVelocityBuffer.AddSample(rawVelocity);

					m_remoteTimeEstimate += m_timeVelocityBuffer.GetTotal() / m_timeVelocityBuffer.Size() * step;
					m_remoteTimeUpdated += step;
					age = (g_time - m_remoteTimeUpdated).GetSeconds();
				}
				else
				{
					//				branch = 5;
					m_remoteTimeUpdated = g_time;
					m_remoteTimeEstimate = r.RemoteTimeAt(g_time);
					m_timeVelocityBuffer.Clear();
					break;
				}
			}
			while (age > 1e-4f);
	}

	//	if (m_lastRemoteTimeEstimate > m_remoteTimeEstimate)
	//		int i=0;
	m_lastRemoteTimeEstimate = m_remoteTimeEstimate;

	return m_remoteTimeEstimate;
}

void CPacketRateCalculator::AddPingSample(CTimeValue nTime, CTimeValue nPing,
                                          CTimeValue nRemoteTime)
{
	if (nRemoteTime == CTimeValue(0.0f))
	{
		NetWarning("[time] zero remote time ignored");
		return;
	}

	m_ping.AddSample(nPing.GetSeconds());
	float cfac = CVARS.RTTConverge / 1000.0f;
	cfac = CLAMP(cfac, 0.5f, 0.999f);
	m_rttEstimate = cfac * m_rttEstimate + (1.0f - cfac) * nPing.GetSeconds();

	m_timeRegression.AddSample(nTime - 0.5f * nPing.GetSeconds(), nRemoteTime);

	/*
	   if (m_nRemoteTimeMeasurement != CTimeValue(0.0f))
	   {
	    CTimeValue nRemoteTimeMeasured = nTime - 0.5f * nPing.GetSeconds();
	    CTimeValue nRemoteTimeAssumed = GetRemoteTime( nRemoteTimeMeasured );
	    CTimeValue nTargetDelta = PingMeasureDelay() + PingMeasureDelay();
	    CTimeValue nRemoteTimeNext = nRemoteTime + nTargetDelta;
	    CTimeValue nAdvance;
	    if (nRemoteTimeNext < nRemoteTimeAssumed)
	    {
	      NetPerformanceWarning("[time] way out of sync (%f %f)", nRemoteTimeNext.GetSeconds(), nRemoteTimeAssumed.GetSeconds() );
	      nAdvance = m_minAdvanceTimeRate;
	      nRemoteTimeAssumed = nRemoteTime;
	    }
	    else if (fabsf((nRemoteTimeAssumed-nRemoteTime).GetSeconds())>1.0f)
	    {
	      NetPerformanceWarning("[time] way out of sync (delta:%f) (%f %f)", (nRemoteTimeAssumed-nRemoteTime).GetSeconds(), nRemoteTime.GetSeconds(), nRemoteTimeAssumed.GetSeconds() );
	      nAdvance = CTimeValue(1.0f);
	      nRemoteTimeAssumed = nRemoteTime;
	    }
	    else
	    {
	      nAdvance = (nRemoteTimeNext - nRemoteTimeAssumed).GetSeconds() / nTargetDelta.GetSeconds();
	      if (nAdvance < m_minAdvanceTimeRate)
	        nAdvance = m_minAdvanceTimeRate;
	      else if (nAdvance > m_maxAdvanceTimeRate)
	        nAdvance = m_maxAdvanceTimeRate;
	    }
	    m_nRemoteTimeMeasurement = nRemoteTimeMeasured;
	    m_nRemoteTimeAdvance = nAdvance;
	    m_nRemoteTimeBase = nRemoteTimeAssumed;
	   }
	   else
	   {
	    m_nRemoteTimeMeasurement = nTime - 0.5f * nPing.GetSeconds();
	    m_nRemoteTimeBase = nRemoteTime;
	   }
	 */
}

uint16 CPacketRateCalculator::GetIdealPacketSize(int age, bool idle, uint16 maxPacketSize)
{
	float sz = NextPacket(age, idle).ps;
	return uint16(Clamp(sz, 0, maxPacketSize) + 0.5f);
}

CTimeValue CPacketRateCalculator::GetNextPacketTime(int age, bool idle)
{
	if (m_bandwidthUsedTime[eIO_Outgoing].Empty())
		return g_time;
	return m_bandwidthUsedTime[eIO_Outgoing].GetLast() + NextPacket(age, idle).dt;
}

void CPacketRateCalculator::SetPerformanceMetrics(const INetChannel::SPerformanceMetrics& metrics)
{
	IConsole* pC = gEnv->pConsole;
	m_metrics = metrics;
	#define SET_DEFAULT(n)   \
	  if (!m_metrics.p ## n) \
	    m_metrics.p ## n = pC->GetCVar("net_defaultChannel" # n);
	SET_DEFAULT(BitRateDesired);
	SET_DEFAULT(BitRateToleranceLow);
	SET_DEFAULT(BitRateToleranceHigh);
	SET_DEFAULT(PacketRateDesired);
	SET_DEFAULT(PacketRateToleranceLow);
	SET_DEFAULT(PacketRateToleranceHigh);
	SET_DEFAULT(IdlePacketRateDesired);
	#undef SET_DEFAULT
}

void CPacketRateCalculator::PacketSizer(SPacketSizerResult& res, const SPacketSizerParams& p)
{
	const float dtDes = 1.0f / p.prDes;
	const float psDes = p.bwDes * dtDes;
	res.dt = (2.0f * psDes * dtDes + p.datCur * dtDes - psDes * p.tCur) / (2.0f * psDes);
	const float dtMax = 1.0f / p.prMin;
	const float dtMin = 1.0f / p.prMax;
	res.dt = Clamp(res.dt, dtMin, dtMax);
	res.ps = (psDes * p.tCur - p.datCur * dtDes + psDes * res.dt) / dtDes;
	const float psMax = dtMax * p.bwMax;
	const float psMin = dtMin * p.bwMin;
	res.ps = Clamp(res.ps, psMin, psMax);
}

static float SlowPacketRateForAge(float des, float low, int age)
{
	if (des < 1)
		return des;

	low = 1.0f / low;
	des = 1.0f / des;

	const float stillFastUntil = 10;
	const float fastImpliesSlowdown = 0.1f;

	const float powFactor = logf(fastImpliesSlowdown * des / (low - des)) / logf(stillFastUntil / 64.0f);
	const float a = powf(age / 64.0f, powFactor);
	return 1.0f / (a * low + (1.0f - a) * des);
}

float CPacketRateCalculator::GetTcpFriendlyBitRate()
{
	float rate = 0.0f;
	if (m_bandwidthUsedAmount[eIO_Outgoing].Empty())
		rate = (float)m_pmtuDiscovery.GetMaxPacketSize(CTimeValue(0.0f)) * 8.f;
	else
	{
		const float sqrt1 = sqrt_tpl(2.0f / 3.0f);
		const float sqrt2 = sqrt_tpl(3.0f / 8.0f);
		float averageSegmentSize = m_bandwidthUsedAmount[eIO_Outgoing].GetAverage();
		const float roundTripTime = m_rttEstimate;
		const float lossEventRate = GetPacketLossPerPacketSent(gEnv->pTimer->GetAsyncTime());
		const float lossEventRateSqr = lossEventRate * lossEventRate;
		const float lossEventRateSqrt = sqrt_tpl(lossEventRate);
		/*
		    if (0.0f == lossEventRate)
		      rate = m_lastSlowStartRate = m_tfrcHist.Empty() ? averageSegmentSize : min( 2.0f*m_tfrcHist.GetLast(), 2.0f*GetMeasuredBitRate() );
		    else
		      rate = min( m_lastSlowStartRate, 8*averageSegmentSize / ( roundTripTime*lossEventRateSqrt*(sqrt1 + 12.0f*sqrt2*lossEventRate*(1.0f+32.0f*lossEventRateSqr) ) ) );
		 */
		if (0.0f == lossEventRate && !m_hadAnyLoss)
		{
			bool allowInc = m_allowSlowStartIncrease;
			if (allowInc)
			{
				if ((g_time - m_lastThroughputCalcTime).GetSeconds() < 1.0f)
					allowInc = false;
				else
					m_lastThroughputCalcTime = g_time;
			}
			rate = m_lastSlowStartRate = m_tfrcHist.Empty() ? averageSegmentSize : (1.0f + (int)allowInc) * m_tfrcHist.GetLast();
			m_allowSlowStartIncrease = false;
		}
		else
		{
			// be a little more aggressive by blending avg. segment size with pmtu
			float aggressiveness = CNetCVars::Get().BandwidthAggressiveness;
			aggressiveness = CLAMP(aggressiveness, 0, 1);
			averageSegmentSize = (1.0f - aggressiveness) * averageSegmentSize + aggressiveness * (m_pmtuDiscovery.GetMaxPacketSize(g_time) + UDP_HEADER_SIZE);
			rate = 8 * averageSegmentSize / (roundTripTime * lossEventRateSqrt * (sqrt1 + 12.0f * sqrt2 * lossEventRate * (1.0f + 32.0f * lossEventRateSqr)));
		}

		//NetLog("rate:%f ass:%f rtt:%f ler:%f lssr:%f hal:%d", rate, averageSegmentSize, roundTripTime, lossEventRate, m_lastSlowStartRate, m_hadAnyLoss);
	}
	// TODO: workaround bug preventing sending of packets if data rate is much too low
	// however, 30kbit/s seems low enough that we should always be able to assume it!
	rate = CLAMP(rate, CVARS.MinTCPFriendlyBitRate, 1e8f);
	m_tfrcHist.AddSample(rate);
	//return m_tfrcHist.GetAverage();
	return rate;
}

CPacketRateCalculator::SPacketSizerResult CPacketRateCalculator::NextPacket(int age, bool idle)
{
	SPacketSizerResult res;

	SPacketSizerParams p;

	float BitRateDesired = 0.0f;
	if (CVARS.EnableTFRC)
		BitRateDesired = GetTcpFriendlyBitRate();
	if (0.0f == BitRateDesired)
		BitRateDesired = m_metrics.pBitRateDesired->GetFVal();

	CalculateDesiredMinMax(
	  BitRateDesired,
	  m_metrics.pBitRateToleranceLow->GetFVal(),
	  m_metrics.pBitRateToleranceHigh->GetFVal(),
	  p.bwDes, p.bwMin, p.bwMax);
	CalculateDesiredMinMax(
	  idle ? m_metrics.pIdlePacketRateDesired : m_metrics.pPacketRateDesired,
	  m_metrics.pPacketRateToleranceLow,
	  m_metrics.pPacketRateToleranceHigh,
	  p.prDes, p.prMin, p.prMax
	  );

	//	const float stalledPacketRate = 2.0f;

	p.bwDes /= 8;
	p.bwMin /= 8;
	p.bwMax /= 8;

	p.prDes = SlowPacketRateForAge(p.prDes, 0.5f, age);
	p.prMin = SlowPacketRateForAge(p.prMin, 0.5f, age);
	p.prMax = SlowPacketRateForAge(p.prMax, 0.5f, age);

	NET_ASSERT(m_bandwidthUsedTime[eIO_Outgoing].Size() == m_bandwidthUsedAmount[eIO_Outgoing].Size());
	if (!m_bandwidthUsedAmount[eIO_Outgoing].Empty())
	{
		p.datCur = (float)m_bandwidthUsedAmount[eIO_Outgoing].GetTotal();
		p.tCur = (m_bandwidthUsedTime[eIO_Outgoing].GetLast() - m_bandwidthUsedTime[eIO_Outgoing].GetFirst()).GetSeconds();
	}
	else
	{
		p.datCur = p.tCur = 0;
	}

	PacketSizer(res, p);

	return res;
}

#endif // !NEW_BANDWIDTH_MANAGEMENT
