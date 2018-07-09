// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  helper class to write network statistics to a file
   -------------------------------------------------------------------------
   History:
   - 13/08/2004   09:29 : Created by Craig Tiller
*************************************************************************/
#include "StdAfx.h"
#include "StatsCollector.h"
#include "Network.h"
#include <CrySystem/ITextModeConsole.h>

#if STATS_COLLECTOR_DEBUG_KIT
	#include "../DebugKit/DebugKit.h"
#endif

#if STATS_COLLECTOR

static const int STATS_FILE_SIZE_BYTES = 1024 * 1024;
static const int STATS_FILE_AMOUNT = 100;
	#define STATS_RESULTS_DIR "NetworkStats"

	#if STATS_COLLECTOR_INTERACTIVE
void CStatsCollector::SStats::Add(float bits)
{
	total += bits;
	mx = max(bits, mx);
	mn = min(bits, mn);
	last = bits;
	count++;
	average = total / count;
	lastUpdate = g_time;
}
	#endif

CStatsCollector::CStatsCollector(const char* filename)
	#if STATS_COLLECTOR_DEBUG_KIT
	: m_debugKit(false)
	#endif
{
	#if STATS_COLLECTOR_FILE
	m_fileId = 0;
	m_file = 0;
	m_fileName = filename;
	CreateLogFile();
	#endif
}

CStatsCollector::~CStatsCollector()
{
	#if STATS_COLLECTOR_FILE
	if (m_file)
		fclose(m_file);
	#endif
}

void CStatsCollector::CreateLogFile()
{
	if (m_file)
	{
		fclose(m_file);
		m_file = 0;
		m_fileId++;
	}
	if (m_fileId > STATS_FILE_AMOUNT)
		m_fileId = 0;

	stack_string fullFilename;

	stack_string dir = gEnv->pSystem->GetRootFolder();
	dir = PathUtil::Make(dir, stack_string(STATS_RESULTS_DIR));
	gEnv->pCryPak->MakeDir(dir);

	fullFilename.Format("%s/%.3d_%s-%s", dir.c_str(), m_fileId, gEnv->pNetwork->GetHostName(), m_fileName.c_str());

	m_file = fxopen(fullFilename.c_str(), "wb");
	if (!m_file)
		NetWarning("[netstats] unable to open '%s' for writing", fullFilename.c_str());
	else
		NetWarning("[netstats] logging to '%s'", fullFilename.c_str());
}

void CStatsCollector::WriteToLogFile(const char* format, ...)
{
	if (!m_file)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(m_file, format, args);
	va_end(args);

	int size = ftell(m_file);
	if (size > STATS_FILE_SIZE_BYTES)
		CreateLogFile();
}

void CStatsCollector::BeginPacket(const TNetAddress& to, CTimeValue nTime, uint32 id)
{
	#if STATS_COLLECTOR_FILE || STATS_COLLECTOR_DEBUG_KIT
	string str_to = RESOLVER.ToString(to);
	#endif

	#if STATS_COLLECTOR_DEBUG_KIT
	if (m_debugKit)
	{
		CDebugKit::Get().SCBegin('bpkt');
		CDebugKit::Get().SCPut(str_to.c_str());
		CDebugKit::Get().SCPut(nTime.GetMilliSeconds());
		CDebugKit::Get().SCPut(id);
	}
	#endif

	#if STATS_COLLECTOR_FILE
	if (!m_file) return;

	WriteToLogFile("BeginPacket %s %u %u\n", str_to, (unsigned)nTime.GetMilliSeconds(), id);
	#endif

	#if STATS_COLLECTOR_INTERACTIVE
	m_components.resize(0);
	m_name.resize(0);
	#endif
}

void CStatsCollector::AddOverheadBits(const char* describe, float bits)
{
	#if 0//STATS_COLLECTOR_DEBUG_KIT
	if (m_debugKit)
	{
		CDebugKit::Get().SCBegin('ovrh');
		CDebugKit::Get().SCPut(describe);
		CDebugKit::Get().SCPut(bits);
	}
	#endif

	#if STATS_COLLECTOR_FILE
	if (!m_file) return;
	WriteToLogFile("Overhead %s %f\n", describe, bits);
	#endif

	#if STATS_COLLECTOR_INTERACTIVE
	if (CNetCVars::Get().ShowDataBits)
		m_stats[m_name + "." + describe].Add(bits);
	#endif
}

void CStatsCollector::Message(const char* message, float bits)
{
	#if 0//STATS_COLLECTOR_DEBUG_KIT
	if (m_debugKit)
	{
		CDebugKit::Get().SCBegin('msg');
		CDebugKit::Get().SCPut(message);
		CDebugKit::Get().SCPut(bits);
	}
	#endif

	#if STATS_COLLECTOR_FILE
	if (!m_file) return;
	WriteToLogFile("Message %s %f\n", message, bits);
	#endif

	#if STATS_COLLECTOR_INTERACTIVE
	if (CNetCVars::Get().ShowDataBits)
		m_stats[m_name + "." + message].Add(bits);
	#endif
}

void CStatsCollector::BeginGroup(const char* name)
{
	#if 0//STATS_COLLECTOR_DEBUG_KIT
	if (m_debugKit)
	{
		CDebugKit::Get().SCBegin('bgrp');
		CDebugKit::Get().SCPut(name);
	}
	#endif

	#if STATS_COLLECTOR_FILE
	if (!m_file) return;
	WriteToLogFile("BeginGroup %s\n", name);
	#endif

	#if STATS_COLLECTOR_INTERACTIVE
	if (CNetCVars::Get().ShowDataBits)
	{
		m_components.push_back(m_name.size());
		m_name.append(".");
		m_name.append(name);
	}
	#endif
}

void CStatsCollector::AddData(const char* name, float bits)
{
	#if STATS_COLLECTOR_DEBUG_KIT
	if (m_debugKit)
	{
		CDebugKit::Get().SCBegin('data');
		CDebugKit::Get().SCPut(name);
		CDebugKit::Get().SCPut(bits);
	}
	#endif

	#if STATS_COLLECTOR_FILE
	if (!m_file) return;
	WriteToLogFile("Data %s %f\n", name, bits);
	#endif

	#if STATS_COLLECTOR_INTERACTIVE
	if (CNetCVars::Get().ShowDataBits)
		m_stats[m_name + "." + name].Add(bits);
	#endif
}

void CStatsCollector::EndGroup()
{
	#if 0//STATS_COLLECTOR_DEBUG_KIT
	if (m_debugKit)
	{
		CDebugKit::Get().SCBegin('egrp');
	}
	#endif

	#if STATS_COLLECTOR_FILE
	if (!m_file) return;
	WriteToLogFile("EndGroup\n");
	#endif

	#if STATS_COLLECTOR_INTERACTIVE
	if (CNetCVars::Get().ShowDataBits)
	{
		m_name.resize(m_components.back());
		m_components.pop_back();
	}
	#endif
}

void CStatsCollector::EndPacket(uint32 nSize)
{
	#if STATS_COLLECTOR_DEBUG_KIT
	if (m_debugKit)
	{
		CDebugKit::Get().SCBegin('epkt');
		CDebugKit::Get().SCPut(nSize);
	}
	#endif

	#if STATS_COLLECTOR_FILE
	if (!m_file) return;
	WriteToLogFile("EndPacket %d\n", nSize);
	#endif
}

void CStatsCollector::LostPacket(uint32 id)
{
	#if 0//STATS_COLLECTOR_DEBUG_KIT
	if (m_debugKit)
	{
		CDebugKit::Get().SCBegin('lost');
		CDebugKit::Get().SCPut(id);
	}
	#endif

	#if STATS_COLLECTOR_FILE
	if (!m_file) return;
	WriteToLogFile("LostPacket %u\n", id);
	#endif
}

	#if STATS_COLLECTOR_INTERACTIVE
struct StatsCompare
{
	int c;

	StatsCompare(int cc) : c(cc) {}

	template<class T>
	bool operator()(const T& a, const T& b) const
	{
		switch (c)
		{
		default:
			return a.first < b.first;
		case 2:
			return a.second.total > b.second.total;
		case 3:
			return a.second.mx > b.second.mx;
		case 4:
			return a.second.mn < b.second.mn;
		case 5:
			return a.second.last > b.second.last;
		case 6:
			return a.second.average > b.second.average;
		case 7:
			return a.second.count > b.second.count;
		}
	}
};

void CStatsCollector::DrawLine(int line, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buf[512];
	cry_vsprintf(buf, fmt, args);
	va_end(args);

	float white[] = { 1, 1, 1, 1 };
	IRenderAuxText::Draw2dLabel(10, line * 10 + 10, 1, white, false, "%s", buf);
	if (ITextModeConsole* pTC = gEnv->pSystem->GetITextModeConsole())
		pTC->PutText(0, line, buf);
}

void CStatsCollector::InteractiveUpdate()
{
	std::vector<std::pair<string, SStats>> toShow;
	for (std::map<string, SStats>::iterator iter = m_stats.begin(); iter != m_stats.end(); )
	{
		std::map<string, SStats>::iterator next = iter;
		++next;
		if (!CNetCVars::Get().ShowDataBits)
			m_stats.erase(iter);
		else
			toShow.push_back(*iter);
		iter = next;
	}
	if (toShow.empty())
		return;
	std::sort(toShow.begin(), toShow.end(), StatsCompare(CNetCVars::Get().ShowDataBits));
	for (size_t i = 0; i < toShow.size(); i++)
	{
		std::pair<string, SStats>& a = toShow[i];
		DrawLine(i, "%s[%d]: %.2f %.2f %.2f %.2f %.2f", a.first.c_str(), a.second.count, a.second.total, a.second.mx, a.second.mn, a.second.last, a.second.average);
	}
}
	#endif

#endif
