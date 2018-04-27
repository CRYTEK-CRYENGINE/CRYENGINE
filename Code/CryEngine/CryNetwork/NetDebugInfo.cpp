// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if ENABLE_NET_DEBUG_INFO

	#include "NetDebugInfo.h"
	#include "NetDebugInternetSimulator.h"
	#include "NetDebugTrafficBandwidth.h"
	#include "NetDebugProfileViewer.h"
	#include "NetDebugServerInfo.h"
	#include "NetDebugChannelViewer.h"
	#include "NetCVars.h"

#include <CryRenderer/IRenderAuxGeom.h>

const float CNetDebugInfoSection::s_white[4] = { 1.0F, 1.0F, 1.0F, 1.0F };
const float CNetDebugInfoSection::s_lgrey[4] = { 0.7f, 0.7f, 0.7f, 1.0f };
const float CNetDebugInfoSection::s_yellow[4] = { 1.0F, 0.8F, 0.0F, 1.0F };
const float CNetDebugInfoSection::s_green[4] = { 0.0F, 0.8F, 0.0F, 1.0F };
const float CNetDebugInfoSection::s_red[4] = { 0.8F, 0.0F, 0.0F, 1.0F };

CNetDebugInfo* CNetDebugInfo::s_pInstance = NULL;

CNetDebugInfoSection::CNetDebugInfoSection(uint32 page, float xpos, float ypos)
	: m_xpos(xpos),
	m_ypos(ypos),
	m_fontSize(1.5F),
	m_lineHeight(16.0F),
	m_sectionHeight(0.0f),
	m_page(page)
{
}

CNetDebugInfoSection::~CNetDebugInfoSection()
{
}

void CNetDebugInfoSection::DrawHeader(const char* sectionHeaderStr)
{
	DrawLine(0, s_yellow, sectionHeaderStr);
}

void CNetDebugInfoSection::DrawLine(uint32 line, const float* pColor, const char* pFormat, ...)
{
	va_list args;

	va_start(args, pFormat);
	VDrawLine(line, pColor, pFormat, args);
	va_end(args);
}

void CNetDebugInfoSection::VDrawLine(uint32 line, const float* pColor, const char* pFormat, va_list args)
{
	float currentHeight = m_lineHeight * line;

	IRenderAuxText::DrawText(Vec3(m_xpos, m_ypos + currentHeight, 0.5f), m_fontSize, pColor, eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Monospace, pFormat, args);

	m_sectionHeight = max(m_lineHeight * (line + 1), currentHeight);
}

void CNetDebugInfoSection::Draw3dLine(const Vec3 pos, uint32 nLine, const float* pColor, const char* pFormat, ...)
{
	va_list args;
	va_start(args, pFormat);

	const Vec3 textLineOffset(0.0f, 0.0f, 0.14f);

	IRenderAuxText::DrawText(pos - (textLineOffset * (float)nLine), 1.25f, pColor, eDrawText_800x600 | eDrawText_FixedSize, pFormat, args);

	va_end(args);
}

CNetDebugObjectsInfo::CNetDebugObjectsInfo()
	: CNetDebugInfoSection(CNetDebugInfo::eP_Test, 50.0F, 50.0F),
	m_boundObjects(CTimeValue(5.0F)),
	m_sendTraffic(CTimeValue(5.0F)),
	m_sendPackets(CTimeValue(5.0F)),
	m_recvTraffic(CTimeValue(5.0F)),
	m_recvPackets(CTimeValue(5.0F))
{
}

CNetDebugObjectsInfo::~CNetDebugObjectsInfo()
{
}

void CNetDebugObjectsInfo::Tick(const SNetworkProfilingStats* const pProfilingStats) PREFAST_SUPPRESS_WARNING(6262)
{
	if (pProfilingStats)
	{
		m_boundObjects.Add(pProfilingStats->m_numBoundObjects);
	}

	SBandwidthStats bandwidthStats;
	CNetwork::Get()->GetBandwidthStatistics(&bandwidthStats);

	SBandwidthStatsSubset deltaStats = bandwidthStats.TickDelta();

	m_sendTraffic.Add((uint32)deltaStats.m_totalBandwidthSent);
	m_sendPackets.Add((uint32)deltaStats.m_totalPacketsSent);
	m_recvTraffic.Add((uint32)deltaStats.m_totalBandwidthRecvd);
	m_recvPackets.Add((uint32)deltaStats.m_totalPacketsRecvd);
}

void CNetDebugObjectsInfo::Draw(const SNetworkProfilingStats* const pProfilingStats)
{
	DrawHeader("BASIC");

	DrawLine(1, s_white, "                     Minimum     Average     Maximum");

	DrawObjects(2, "Bound Objects:", m_boundObjects.m_count, m_boundObjects.m_min, m_boundObjects.m_average, m_boundObjects.m_max);
	DrawObjects(3, "Sent bits/tick:", m_sendTraffic.m_count, m_sendTraffic.m_min, m_sendTraffic.m_average, m_sendTraffic.m_max);
	DrawObjects(4, "Sent pkts/tick:", m_sendPackets.m_count, m_sendPackets.m_min, m_sendPackets.m_average, m_sendPackets.m_max);
	DrawObjects(5, "Recv bits/tick:", m_recvTraffic.m_count, m_recvTraffic.m_min, m_recvTraffic.m_average, m_recvTraffic.m_max);
	DrawObjects(6, "Recv pkts/tick:", m_recvPackets.m_count, m_recvPackets.m_min, m_recvPackets.m_average, m_recvPackets.m_max);
}

void CNetDebugObjectsInfo::DrawObjects(uint32 line, const char* pLabel, uint32 count, uint32 minVal, float average, uint32 maxVal)
{
	DrawLine(line, s_white, "%16s %11u %11.3f %11u", pLabel, minVal, average, maxVal);
}

CNetDebugInfo::CNetDebugInfo()
{
	m_sections.push_back(new CNetDebugObjectsInfo());
	m_sections.push_back(new CNetDebugInternetSimulator());
	m_sections.push_back(new CNetDebugTrafficBandwidth());
	#if NET_PROFILE_ENABLE
	m_sections.push_back(new CNetDebugProfileViewer(eP_ProfileViewLastSecond));
	m_sections.push_back(new CNetDebugProfileViewer(eP_ProfileViewLast5Seconds));
	m_sections.push_back(new CNetDebugProfileViewer(eP_ProfileViewOnEntities));
	#endif
	m_sections.push_back(new CNetDebugServerInfo());
	m_sections.push_back(new CNetDebugChannelViewer());
	m_sections.push_back(new CNetDebugChannelViewerAccountingGroupsOverview());
	m_sections.push_back(new CNetDebugChannelViewerAccountingGroupsSends());
}

CNetDebugInfo::~CNetDebugInfo()
{
	for (std::vector<CNetDebugInfoSection*>::iterator it = m_sections.begin(); it != m_sections.end(); it++)
	{
		delete (*it);
	}
	m_sections.clear();
}

CNetDebugInfo* CNetDebugInfo::Get()
{
	if (!s_pInstance)
	{
		s_pInstance = new CNetDebugInfo();
	}

	return s_pInstance;
}

void CNetDebugInfo::Tick()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	if (CNetCVars::Get().netDebugInfo != 0)
	{
		SNetworkProfilingStats debugStats;
		CNetwork::Get()->GetProfilingStatistics(&debugStats);

		for (std::vector<CNetDebugInfoSection*>::iterator it = m_sections.begin(); it != m_sections.end(); it++)
		{
			(*it)->Tick(&debugStats);

			if ((EPage)((*it)->GetPage()) == CNetCVars::Get().netDebugInfo)
			{
				(*it)->Draw(&debugStats);
			}
		}
	}
}

#endif
