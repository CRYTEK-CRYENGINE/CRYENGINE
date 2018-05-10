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
#ifndef __STATSCOLLECTOR_H__
#define __STATSCOLLECTOR_H__

#pragma once

#include <CrySystem/TimeValue.h>
#include "Config.h"
#include <CryNetwork/NetAddress.h>

#if STATS_COLLECTOR

class CStatsCollector
{
public:
	CStatsCollector(const char* filename = "netstats.log");
	~CStatsCollector();

	void BeginPacket(const TNetAddress& to, CTimeValue nTime, uint32 id);
	void AddOverheadBits(const char* describe, float bits);
	void Message(const char* message, float bits);
	void BeginGroup(const char* name);
	void AddData(const char* name, float bits);
	void EndGroup();
	void EndPacket(uint32 nSize);

	void LostPacket(uint32 id);

	#if STATS_COLLECTOR_DEBUG_KIT
	void SetDebugKit(bool on) { m_debugKit = on; }
	#endif

	#if STATS_COLLECTOR_INTERACTIVE
	void InteractiveUpdate();
	#endif

private:
	#if STATS_COLLECTOR_FILE
	FILE * m_file;
	int    m_fileId;
	string m_fileName;
	void WriteToLogFile(const char* format, ...);
	void CreateLogFile();
	#endif
	#if STATS_COLLECTOR_DEBUG_KIT
	bool m_debugKit;
	#endif

	#if STATS_COLLECTOR_INTERACTIVE
	std::vector<uint32> m_components;
	string m_name;

	struct SStats
	{
		SStats() : last(0), total(0), mx(0), mn(1e12f), count(0), average(0) {}
		void Add(float);
		CTimeValue lastUpdate;
		float      total;
		float      mx;
		float      mn;
		float      last;
		float      average;
		int        count;
	};
	std::map<string, SStats> m_stats;

	void DrawLine(int line, const char* fmt, ...);
	#endif
};

#else // !_STATS_COLLECTOR

class CStatsCollector
{
public:
	ILINE CStatsCollector(const char* filename = "") {}
	ILINE void BeginPacket(const TNetAddress& to, CTimeValue nTime, uint32 id) {}
	ILINE void AddOverheadBits(const char* describe, float bits)               {}
	ILINE void Message(const char* message, float bits)                        {}
	ILINE void BeginGroup(const char* name)                                    {}
	ILINE void AddData(const char* name, float bits)                           {}
	ILINE void EndGroup()                                                      {}
	ILINE void EndPacket(uint32 nSize)                                         {}
	ILINE void LostPacket(uint32 id)                                           {}
	ILINE void SetDebugKit(bool)                                               {}
};

#endif // _STATS_COLLECTOR

#endif
