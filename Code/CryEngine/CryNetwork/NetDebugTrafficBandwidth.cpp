// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if ENABLE_NET_DEBUG_INFO

	#include "NetDebugTrafficBandwidth.h"
	#include "NetCVars.h"

CNetDebugTrafficBandwidth::CNetDebugTrafficBandwidth()
	: CNetDebugInfoSection(CNetDebugInfo::eP_Test, 50.0F, 360.0F)
{
}

CNetDebugTrafficBandwidth::~CNetDebugTrafficBandwidth()
{
}

void CNetDebugTrafficBandwidth::Tick(const SNetworkProfilingStats* const pProfilingStats)
{
	CNetwork::Get()->GetBandwidthStatistics(&m_bandwidthStats);
}

void CNetDebugTrafficBandwidth::Draw(const SNetworkProfilingStats* const pProfilingStats)
{
	float invKB = 1.0f / 1024.0f;

	DrawHeader("TRAFFIC BANDWIDTH");

	SBandwidthStatsSubset delta = m_bandwidthStats.TickDelta();

	DrawLine(1, s_white, "Last Network tick:     %5d bits (%3d packets) SENT, %5d bits RECV (%3d packets)", delta.m_totalBandwidthSent, delta.m_totalPacketsSent, delta.m_totalBandwidthRecvd, delta.m_totalPacketsRecvd);
	DrawLine(2, s_white, "Total Bandwidth Stats SENT: %0.2f Kbits (%d packets @ %" PRId64 " Avg bits/packet)",
	         (float)m_bandwidthStats.m_total.m_totalBandwidthSent * invKB, m_bandwidthStats.m_total.m_totalPacketsSent, m_bandwidthStats.m_total.m_totalBandwidthSent / (uint64)max(m_bandwidthStats.m_total.m_totalPacketsSent, 1));
	DrawLine(3, s_white, "Total Bandwidth Stats RECV: %0.2f Kbits (%d packets @ %" PRId64 " Avg bits/packet)",
	         (float)m_bandwidthStats.m_total.m_totalBandwidthRecvd * invKB, m_bandwidthStats.m_total.m_totalPacketsRecvd, m_bandwidthStats.m_total.m_totalBandwidthRecvd / (uint64)max(m_bandwidthStats.m_total.m_totalPacketsRecvd, 1));

	uint64 deltaOtherBits = delta.m_totalBandwidthSent - delta.m_lobbyBandwidthSent - delta.m_seqBandwidthSent - delta.m_fragmentBandwidthSent;
	uint32 numOtherPackets = m_bandwidthStats.m_1secAvg.m_totalPacketsSent - m_bandwidthStats.m_1secAvg.m_lobbyPacketsSent - m_bandwidthStats.m_1secAvg.m_seqPacketsSent;
	float oneSecOtherSent = (float)(m_bandwidthStats.m_1secAvg.m_totalBandwidthSent - m_bandwidthStats.m_1secAvg.m_lobbyBandwidthSent - m_bandwidthStats.m_1secAvg.m_seqBandwidthSent);
	float tenSecOtherSent = (float)(m_bandwidthStats.m_10secAvg.m_totalBandwidthSent - m_bandwidthStats.m_10secAvg.m_lobbyBandwidthSent - m_bandwidthStats.m_10secAvg.m_seqBandwidthSent);

	DrawLine(5, s_white, "Send           Bits last frame       Packets/sec        Kbits/sec     Kbits/sec (10 sec avg)");
	DrawLine(6, s_white, "%12s %18" PRId64 " %16d %16.2f %16.2f", "Lobby", delta.m_lobbyBandwidthSent, m_bandwidthStats.m_1secAvg.m_lobbyPacketsSent,
	         (float)m_bandwidthStats.m_1secAvg.m_lobbyBandwidthSent * invKB,
	         (float)m_bandwidthStats.m_10secAvg.m_lobbyBandwidthSent * invKB);
	DrawLine(7, s_white, "%12s %18" PRId64 " %16d %16.2f %16.2f", "Sequence", delta.m_seqBandwidthSent, m_bandwidthStats.m_1secAvg.m_seqPacketsSent,
	         (float)m_bandwidthStats.m_1secAvg.m_seqBandwidthSent * invKB,
	         (float)m_bandwidthStats.m_10secAvg.m_seqBandwidthSent * invKB);
	DrawLine(8, s_white, "%12s %18" PRId64 " %16d %16.2f %16.2f", "Fragmented", delta.m_fragmentBandwidthSent, m_bandwidthStats.m_1secAvg.m_fragmentPacketsSent,
	         (float)m_bandwidthStats.m_1secAvg.m_fragmentBandwidthSent * invKB,
	         (float)m_bandwidthStats.m_10secAvg.m_fragmentBandwidthSent * invKB);
	DrawLine(9, s_white, "%12s %18" PRId64 " %16d %16.2f %16.2f", "Other", deltaOtherBits, numOtherPackets,
	         oneSecOtherSent * invKB, tenSecOtherSent * invKB);
	DrawLine(10, s_white, "%12s %18" PRId64 " %16d %16.2f %16.2f", "All", delta.m_totalBandwidthSent, m_bandwidthStats.m_1secAvg.m_totalPacketsSent,
	         (float)m_bandwidthStats.m_1secAvg.m_totalBandwidthSent * invKB,
	         (float)m_bandwidthStats.m_10secAvg.m_totalBandwidthSent * invKB);
	DrawLine(11, s_white, "%12s %18" PRId64 " %16d %16.2f %16.2f", "Recvd", delta.m_totalBandwidthRecvd, m_bandwidthStats.m_1secAvg.m_totalPacketsRecvd,
	         (float)m_bandwidthStats.m_1secAvg.m_totalBandwidthRecvd * invKB,
	         (float)m_bandwidthStats.m_10secAvg.m_totalBandwidthRecvd * invKB);
}

#endif
