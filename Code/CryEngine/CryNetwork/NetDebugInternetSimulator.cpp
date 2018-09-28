// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if ENABLE_NET_DEBUG_INFO

	#include "NetDebugInternetSimulator.h"
	#include "NetCVars.h"

CNetDebugInternetSimulator::CNetDebugInternetSimulator()
	: CNetDebugInfoSection(CNetDebugInfo::eP_Test, 50.0F, 200.0F),
	m_lagAverage(CTimeValue(5)), m_packetDrops(0), m_packetSends(0), m_packetLossRate(0.0f), m_packetLagMin(0), m_packetLagMax(0)
{
}

CNetDebugInternetSimulator::~CNetDebugInternetSimulator()
{
}

void CNetDebugInternetSimulator::Tick(const SNetworkProfilingStats* const pProfilingStats)
{
	#if INTERNET_SIMULATOR
	m_packetLossRate = CVARS.net_PacketLossRate;
	if (pProfilingStats)
	{
		m_packetSends = pProfilingStats->m_InternetSimulatorStats.m_packetSends;
		m_packetDrops = pProfilingStats->m_InternetSimulatorStats.m_packetDrops;
	}

	m_packetLagMin = CVARS.net_PacketLagMin;
	m_packetLagMax = CVARS.net_PacketLagMax;

	assert(pProfilingStats);
	PREFAST_ASSUME(pProfilingStats);
	m_lagAverage.Add((int32)pProfilingStats->m_InternetSimulatorStats.m_lastPacketLag.GetMilliSeconds());
	#endif
}

void CNetDebugInternetSimulator::Draw(const SNetworkProfilingStats* const pProfilingStats)
{
	DrawHeader("INTERNET SIMULATOR");

	DrawLine(1, s_white, "Simulated Loss: %.2f%% chance of packet loss.", m_packetLossRate);
	DrawLine(2, s_white, "                %d attempted sends, %d dropped, %.2f actual loss.", m_packetSends, m_packetDrops, (float)m_packetDrops / max((float)m_packetSends, 1.0f));

	DrawLine(4, s_white, "Simulated Lag:  %d-%dms packet lag range.", m_packetLagMin, m_packetLagMax);
	// Inaccurate due to millisecond rounding + float averaging in accumulator.
	DrawLine(5, s_white, "                %.2fms rolling lag average", m_lagAverage.m_average);

	CNetChannel* pChannel = CNetwork::Get()->FindFirstRemoteChannel();
	CTimeValue ping = 0;
	if (pChannel)
	{
		ping = pChannel->GetPing(true);
	}

	DrawLine(7, s_white, "Real average PING: %sms", ping.GetMilliSeconds().str());
}

#endif
