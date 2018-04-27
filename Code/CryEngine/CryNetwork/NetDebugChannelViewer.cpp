// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if ENABLE_NET_DEBUG_INFO

	#include "NetDebugChannelViewer.h"
	#include <CryGame/IGameFramework.h>
	#include <CrySystem/ISystem.h>

///////////////////////////////////////////////////////////////////////////////

CNetDebugChannelViewer::CNetDebugChannelViewer()
	: CNetDebugInfoSection(CNetDebugInfo::eP_NetChannelOverview, 0.0f, 0.0f)
{
}

///////////////////////////////////////////////////////////////////////////////

CNetDebugChannelViewer::~CNetDebugChannelViewer()
{
}

///////////////////////////////////////////////////////////////////////////////

void CNetDebugChannelViewer::Draw(const SNetworkProfilingStats* const pNetworkProfilingStats) PREFAST_SUPPRESS_WARNING(6262)
{
	DrawHeader("NetChannel Overview");
	SBandwidthStats stats;
	gEnv->pNetwork->GetBandwidthStatistics(&stats);

	uint32 channelIndex = 0;
	uint32 line = 1;

	DrawLine(line++, s_white, "                 :  Ping (ms)  :  Bandwidth (KiB)       : Packet Rate            :  Packet Size :");
	DrawLine(line++, s_white, "Name             : Curr : Avg  :    In   /   Out   Shrs : Aim / Current  / Loss%% : Max  / Ideal : Idle");

	while (channelIndex < STATS_MAX_NUMBER_OF_CHANNELS)
	{
		const SNetChannelStats& channelStats = stats.m_channel[channelIndex++];
		if (channelStats.m_inUse)
		{
			DrawLine(line++, s_white, "%16s : %4d : %4d : %7.02f / %7.02f [%2d] :  %d / %8.02f / %5.02f : %4d / %4d  :  %c",
			         channelStats.m_name,
			         channelStats.m_ping,
			         channelStats.m_pingSmoothed,
			         channelStats.m_bandwidthInbound,
			         channelStats.m_bandwidthOutbound,
			         channelStats.m_bandwidthShares,
			         channelStats.m_desiredPacketRate,
			         channelStats.m_currentPacketRate,
			         channelStats.m_packetLossRate,
			         channelStats.m_maxPacketSize,
			         channelStats.m_idealPacketSize,
			         (channelStats.m_idle) ? 'Y' : 'N');
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

CNetDebugChannelViewerAccountingGroupsOverview::CNetDebugChannelViewerAccountingGroupsOverview()
	: CNetDebugInfoSection(CNetDebugInfo::eP_NetChannelAccountingGroupsOverview, 0.0f, 0.0f)
{
}

///////////////////////////////////////////////////////////////////////////////

CNetDebugChannelViewerAccountingGroupsOverview::~CNetDebugChannelViewerAccountingGroupsOverview()
{
}

///////////////////////////////////////////////////////////////////////////////

void CNetDebugChannelViewerAccountingGroupsOverview::Draw(const SNetworkProfilingStats* const pNetworkProfilingStats) PREFAST_SUPPRESS_WARNING(6262)
{
	DrawHeader("Accounting Groups Overview");
	SBandwidthStats stats;
	gEnv->pNetwork->GetBandwidthStatistics(&stats);

	uint32 channelIndex = CNetCVars::Get().netDebugChannelIndex;
	if (channelIndex < STATS_MAX_NUMBER_OF_CHANNELS)
	{
		const SNetChannelStats& channelStats = stats.m_channel[channelIndex];
		if (channelStats.m_inUse)
		{
			DrawLine(1, s_white, "Channel Name: %s", channelStats.m_name);
			DrawLine(3, s_white, "Name : BandWidth : Total Bandwidth : Priority : Max Latency : Discard Latency");

			uint32 line = 4;
			uint32 accountingGroupIndex = 0;

			while (accountingGroupIndex < STATS_MAX_MESSAGEQUEUE_ACCOUNTING_GROUPS)
			{
				const SAccountingGroupStats& accountingGroupStats = channelStats.m_messageQueue.m_accountingGroup[accountingGroupIndex++];
				if (accountingGroupStats.m_inUse)
				{
					DrawLine(line++, s_white, "%4s :  %8.02f :     %8.02f    :    %2d    :     % 4.02f    :      % 4.02f",
					         accountingGroupStats.m_name,
					         accountingGroupStats.m_bandwidthUsed,
					         accountingGroupStats.m_totalBandwidthUsed,
					         accountingGroupStats.m_priority,
					         accountingGroupStats.m_maxLatency,
					         accountingGroupStats.m_discardLatency);
				}
			}
		}
		else
		{
			DrawLine(1, s_red, "Channel index %d (net_debugChannelIndex) not in use", channelIndex);
		}
	}
	else
	{
		DrawLine(1, s_red, "Invalid channel index set in console variable net_debugChannelIndex");
	}
}

///////////////////////////////////////////////////////////////////////////////

CNetDebugChannelViewerAccountingGroupsSends::CNetDebugChannelViewerAccountingGroupsSends()
	: CNetDebugInfoSection(CNetDebugInfo::eP_NetChannelAccountingGroupsSends, 0.0f, 0.0f)
{
}

///////////////////////////////////////////////////////////////////////////////

CNetDebugChannelViewerAccountingGroupsSends::~CNetDebugChannelViewerAccountingGroupsSends()
{
}

///////////////////////////////////////////////////////////////////////////////

void CNetDebugChannelViewerAccountingGroupsSends::Draw(const SNetworkProfilingStats* const pNetworkProfilingStats) PREFAST_SUPPRESS_WARNING(6262)
{
	DrawHeader("Accounting Groups Sends");
	SBandwidthStats stats;
	gEnv->pNetwork->GetBandwidthStatistics(&stats);

	uint32 channelIndex = CNetCVars::Get().netDebugChannelIndex;
	if (channelIndex < STATS_MAX_NUMBER_OF_CHANNELS)
	{
		const SNetChannelStats& channelStats = stats.m_channel[channelIndex];
		if (channelStats.m_inUse)
		{
			DrawLine(1, s_white, "Channel Name: %s", channelStats.m_name);
			DrawLine(3, s_white, "Name : Sends : BandWidth");

			uint32 line = 4;
			uint32 accountingGroupIndex = 0;
			while (accountingGroupIndex < STATS_MAX_MESSAGEQUEUE_ACCOUNTING_GROUPS)
			{
				const SAccountingGroupStats& accountingGroupStats = channelStats.m_messageQueue.m_accountingGroup[accountingGroupIndex++];
				if (accountingGroupStats.m_inUse)
				{
					DrawLine(line++, s_white, "%4s :   %2d  : %8.02f", accountingGroupStats.m_name, accountingGroupStats.m_sends, accountingGroupStats.m_bandwidthUsed);
				}
			}
		}
		else
		{
			DrawLine(1, s_red, "Channel index %d (net_debugChannelIndex) not in use", channelIndex);
		}
	}
	else
	{
		DrawLine(1, s_red, "Invalid channel index set in console variable net_debugChannelIndex");
	}
}

///////////////////////////////////////////////////////////////////////////////

#endif // ENABLE_NET_DEBUG_INFO
