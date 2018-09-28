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
#include <CryGame/IGameFramework.h>

#if NEW_BANDWIDTH_MANAGEMENT

	#undef min
	#undef max

static float Clamp(float x, float mn, float mx)
{
	if (x < mn) return mn;
	if (x > mx) return mx;
	return x;
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

CPacketRateCalculator::CPacketRateCalculator() :
	m_sentPackets(5),
	m_lostPackets(5),
	m_minAdvanceTimeRate("0.8"),
	m_maxAdvanceTimeRate("1.2"),
	m_lastSlowStartRate(0.0f),
	m_hadAnyLoss(false),
	m_allowSlowStartIncrease(false),
	m_latencyLabHighestSequence(0),
	m_remoteTimeEstimate(0)
{
	m_lastThroughputCalcTime.SetSeconds(0);
	m_rttEstimate.SetSeconds(1);

	m_metrics.m_bandwidthShares = CNetCVars::Get().net_defaultBandwidthShares;
	m_metrics.m_packetRate = CNetCVars::Get().net_defaultPacketRate;
	m_metrics.m_packetRateIdle = CNetCVars::Get().net_defaultPacketRateIdle;
}

CTimeValue CPacketRateCalculator::GetPing(bool smoothed) const
{
	if (m_ping.Empty())
		return 0;
	else if (smoothed)
		return m_ping.GetAverageT();
	else
		return m_ping.GetLast();
}

float CPacketRateCalculator::GetBandwidthUsage(CTimeValue nTime, CPacketRateCalculator::eIncomingOutgoing direction)
{
	NET_ASSERT(m_bandwidthUsedTime[direction].Size() == m_bandwidthUsedAmount[direction].Size());

	uint32 total = m_bandwidthUsedAmount[direction].GetTotal();
	CTimeValue time = m_bandwidthUsedTime[direction].Empty() ? 1 : nTime - m_bandwidthUsedTime[direction].GetFirst();

	// ensure time >= 1
	if (time < 1)
		time.SetSeconds(1);

	return BADF(total / time.GetSeconds());
}

float CPacketRateCalculator::GetPacketLossPerSecond(CTimeValue nTime)
{
	return m_lostPackets.CountsPerSecond(nTime);
}

	#pragma warning(push)
	#pragma warning(disable:28285)
float CPacketRateCalculator::GetPacketLossPerPacketSent(CTimeValue nTime)
{
	if (m_sentPackets.Empty() || m_lostPackets.Empty())
		return 0.0f;

	CTimeValue firstTime = std::max(m_sentPackets.FirstSampleTime(), m_lostPackets.FirstSampleTime());

	float pktRate = m_sentPackets.CountsPerSecond(nTime, firstTime);
	if (pktRate == 0.0f)
	{
		return 0.0f;
	}

	float lossRate = m_lostPackets.CountsPerSecond(nTime, firstTime);
	return lossRate / pktRate;
}
	#pragma warning(pop)

float CPacketRateCalculator::GetPacketRate(bool idle, CTimeValue nTime)
{
	return m_metrics.m_packetRate / std::max(0.01f, 1.0f - GetPacketLossPerPacketSent(nTime));
}

void CPacketRateCalculator::SentPacket(CTimeValue nTime, uint32 nSeq, uint16 nSize)
{
	m_sentPackets.AddSample(nTime);
	m_bandwidthUsedTime[eIO_Outgoing].AddSample(nTime);
	m_bandwidthUsedAmount[eIO_Outgoing].AddSample(nSize + UDP_HEADER_SIZE);

	// TODO: make sure nTime is the current time, otherwise we have to use GetGTimer()->GetAsyncTime()
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

	// TODO: make sure nTime is the current time, otherwise we have to use GetGTimer()->GetAsyncTime()
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
				if (m_highLatencyStartTime == 0)
					m_highLatencyStartTime = nTime;
			}
			else
				m_highLatencyStartTime.SetSeconds(0);
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
	// TODO: make sure nTime is the current time, otherwise we need to use GetGTimer()->GetAsyncTime()
	MiniQueue<CTimeValue, 127>::SIterator itor = m_latencyLab.Begin();
	for (; itor != m_latencyLab.End(); ++itor)
	{
		CTimeValue latency = nTime - *itor;
		if (latency < CVARS.HighLatencyThreshold)
			break;

		if (m_highLatencyStartTime == 0)
			m_highLatencyStartTime = nTime;
	}

	m_latencyLab.Erase(m_latencyLab.Begin(), itor);
}

bool CPacketRateCalculator::IsSufferingHighLatency(CTimeValue nTime) const
{
	if (CVARS.HighLatencyThreshold <= 0)
		return false;

	if (m_highLatencyStartTime == 0)
		return false;

	return nTime - m_highLatencyStartTime >= CVARS.HighLatencyTimeLimit;
}

CTimeValue CPacketRateCalculator::GetRemoteTime() const
{
	static const CTimeValue MAX_STEP_SIZE = "0.01";

	TTimeRegression::CResult r;

	CTimeValue age = g_time - m_remoteTimeUpdated;
	if (abs(age) > "0.5")
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
			m_remoteTimeUpdated.SetSeconds(0);
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
			m_remoteTimeUpdated.SetSeconds(0);
		}
		else
			do
			{
				//			branch = 4;
				CTimeValue step = std::min(age, MAX_STEP_SIZE);
				const CTimeValue targetPos = r.RemoteTimeAt(m_remoteTimeUpdated);

				if (abs((m_remoteTimeEstimate - targetPos).GetSeconds()) < "1.25")
				{
					const mpfloat targetVel = r.GetSlope();
					const mpfloat maxVel = 20;
					const mpfloat minVel = 1 / maxVel;

					const mpfloat rawVelocity = CLAMP(targetVel + (targetPos - m_remoteTimeEstimate).GetSeconds(), minVel, maxVel);
					m_timeVelocityBuffer.AddSample(rawVelocity);

					m_remoteTimeEstimate += m_timeVelocityBuffer.GetTotal() / m_timeVelocityBuffer.Size() * step;
					m_remoteTimeUpdated  += step;
					age = g_time - m_remoteTimeUpdated;
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
			while (age > "0.0001");// 1e-4 MP_SCIFI
	}

	//	if (m_lastRemoteTimeEstimate > m_remoteTimeEstimate)
	//		int i=0;
	m_lastRemoteTimeEstimate = m_remoteTimeEstimate;

	return m_remoteTimeEstimate;
}

void CPacketRateCalculator::AddPingSample(CTimeValue nTime, CTimeValue nPing,
                                          CTimeValue nRemoteTime)
{
	if (nRemoteTime == CTimeValue(0))
	{
		NetWarning("[time] zero remote time ignored");
		return;
	}

	m_ping.AddSample(nPing);
	m_rttEstimate = m_ping.GetAverageT();

	m_timeRegression.AddSample(nTime - "0.5" * nPing, nRemoteTime);
}

TPacketSize CPacketRateCalculator::GetIdealPacketSize(const CTimeValue time, bool idle, TPacketSize maxPacketSize)
{
	const float availableBandwidth = GetAvailableBandwidth() * 1000.0f / 8.0f; // Expressed as bytes
	float packetsPerSecond = 0.0f;
	float bytesPerSecond = 0.0f;

	CalculateCurrentBandwidth(time, packetsPerSecond, bytesPerSecond);

	if (packetsPerSecond < 1.0f)
	{
		packetsPerSecond = 1.0f;
	}

	float packetRate;
	if (!idle)
	{
		packetRate = std::min(static_cast<float>(m_metrics.m_packetRate), packetsPerSecond);
	}
	else
	{
		packetRate = std::min(static_cast<float>(m_metrics.m_packetRateIdle), packetsPerSecond);
	}

	TPacketSize idealPacketSize = static_cast<TPacketSize>(Clamp(availableBandwidth / packetRate, 0, static_cast<float>(maxPacketSize)) + 0.5f);
	return idealPacketSize;
}

TPacketSize CPacketRateCalculator::GetSparePacketSize(const CTimeValue time, TPacketSize idealPacketSize, TPacketSize maxPacketSize
	#if LOG_BANDWIDTH_SHAPING
                                                      , bool isLocal, const char* name
	#endif // LOG_BANDWIDTH_SHAPING
                                                      )
{
	const float availableBandwidth = GetAvailableBandwidth() * 1000.0f / 8.0f; // Expressed as bytes
	float packetsPerSecond = 0.0f;
	float bytesPerSecond = 0.0f;

	CalculateCurrentBandwidth(time, packetsPerSecond, bytesPerSecond);

	TPacketSize spareCapacity = static_cast<TPacketSize>(Clamp(availableBandwidth - bytesPerSecond, 0, static_cast<float>(maxPacketSize - idealPacketSize)));

	#if LOG_BANDWIDTH_SHAPING
	if (!isLocal && (bytesPerSecond > availableBandwidth))
	{
		NetQuickLog(true, 5.0f, "[Bandwidth] [%s] available/s %.2f, bytes/s %.2f, packets/s %.2f", name, availableBandwidth, bytesPerSecond, packetsPerSecond);
		NetLog("[Bandwidth] [%s] ideal %d, max %d, spare %d", name, idealPacketSize, maxPacketSize, spareCapacity);
	}
	#endif // LOG_BANDWIDTH_SHAPING

	return spareCapacity;
}

void CPacketRateCalculator::CalculateCurrentBandwidth(const CTimeValue time, float& packetsPerSecond, float& bytesPerSecond)
{
	const size_t numSamples = m_bandwidthUsedTime[eIO_Outgoing].Size();
	uint32 index = 0;
	float bytesSent = 0.0f;
	float packetsSent = 0.0f;

	// Walk back through the sample timestamps for the last second
	const CTimeValue desiredTimePeriod = 1;
	CTimeValue actualTimePeriod = 0;
	CTimeValue timeDiff = 0;
	while ((index < numSamples) && (timeDiff < desiredTimePeriod))
	{
		const CTimeValue& sampleTime = m_bandwidthUsedTime[eIO_Outgoing][numSamples - index];
		timeDiff = time - sampleTime;
		if (timeDiff < desiredTimePeriod)
		{
			bytesSent += m_bandwidthUsedAmount[eIO_Outgoing][numSamples - index];
			++packetsSent;
			actualTimePeriod = timeDiff;
		}
		++index;
	}

	if (actualTimePeriod > 0)
	{
		packetsPerSecond = packetsSent / actualTimePeriod.BADGetSeconds();
		bytesPerSecond = bytesSent / actualTimePeriod.BADGetSeconds();
	}
	else
	{
		packetsPerSecond = 0.0f;
		bytesPerSecond = 0.0f;
	}
}

CTimeValue CPacketRateCalculator::GetNextPacketTime(int age, bool idle)
{
	if (m_bandwidthUsedTime[eIO_Outgoing].Empty())
		return g_time;

	uint32 packetRate;
	if (!idle)
	{
		packetRate = m_metrics.m_packetRate;
	}
	else
	{
		packetRate = m_metrics.m_packetRateIdle;
	}
	CTimeValue desiredInterval = CTimeValue(1 / mpfloat(packetRate));

	CTimeValue next = m_bandwidthUsedTime[eIO_Outgoing].GetFirst() + desiredInterval;
	if (next < g_time)
	{
		next = g_time;
	}

	return next;
}

void CPacketRateCalculator::SetPerformanceMetrics(const INetChannel::SPerformanceMetrics& metrics)
{
	// Update our metrics from the passed metrics, but only for valid entries
	// - allows caller to only specify what they want to change
	#define SET_ALTERED(n) \
	  if (metrics.n != INVALID_PERFORMANCE_METRIC) { m_metrics.n = metrics.n; }
	SET_ALTERED(m_bandwidthShares);
	SET_ALTERED(m_packetRate);
	SET_ALTERED(m_packetRateIdle);
	#undef SET_ALTERED
}

float CPacketRateCalculator::GetAvailableBandwidth() const
{
	float availableBandwidth = 0;
	CNetNub* pNetNub = (CNetNub*)gEnv->pGameFramework->GetServerNetNub();

	if (pNetNub != NULL)
	{
		// We're a server
		float totalBandwidthShares = static_cast<float>(pNetNub->GetTotalBandwidthShares());
		float channelBandwidthShares = min(totalBandwidthShares, static_cast<float>(m_metrics.m_bandwidthShares));
		availableBandwidth = (CNetCVars::Get().net_availableBandwidthServer / totalBandwidthShares) * channelBandwidthShares;
	}
	else
	{
		// We're a client
		availableBandwidth = CNetCVars::Get().net_availableBandwidthClient;
	}

	return availableBandwidth;
}
#endif // NEW_BANDWIDTH_MANAGEMENT
