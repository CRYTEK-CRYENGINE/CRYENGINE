// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id: DiskProfiler.h,v 1.0 2008/03/28 11:11:13 AntonKaplanyan Exp wwwrun $
   $DateTime$
   Description:  Routine for profiling disk IO
   -------------------------------------------------------------------------
   History:
   - 28:3:2008   11:11 : Created by Anton Kaplanyan
*************************************************************************/

#ifndef __diskprofilesystem_h__
#define __diskprofilesystem_h__
#pragma once

#ifdef USE_DISK_PROFILER

#include <CrySystem/Profilers/IDiskProfiler.h>
#include <CrySystem/IStreamEngine.h>
#include <CryMath/Cry_Color.h>

class CDiskProfilerWindowsSpecific;

//////////////////////////////////////////////////////////////////////////
// Disk Profile main class
//////////////////////////////////////////////////////////////////////////
class CDiskProfiler : public IDiskProfiler
{
	friend class CDiskProfileTimer;

public:
	CDiskProfiler(ISystem* pSystem);
	~CDiskProfiler();
	virtual void Update();  // perframe update routine
	virtual DiskOperationInfo GetStatistics() const { return m_outStatistics; }
	virtual bool RegisterStatistics(SDiskProfileStatistics* pStatistics);  // main statistics registering function

	virtual void SetTaskType(const threadID nThreadId, const uint32 nType = 0xffffffff);  // task type registering function

	virtual bool IsEnabled() const;

protected:
	// rendering routine
	void RenderBlock(const float timeStart, const float timeEnd, const ColorB threadColor, const ColorB IOTypeColor);
	void Render();

	typedef std::deque<SDiskProfileStatistics*> Statistics;
	typedef std::map<threadID, ColorB> ThreadColorMap;
	typedef std::map<threadID, EStreamTaskType, std::less<threadID>, stl::STLGlobalAllocator<std::pair<const threadID, EStreamTaskType>>> ThreadTaskTypeMap;

	volatile bool m_bEnabled;
	CryCriticalSection m_csLock;  // MT-safe profiling
	Statistics m_statistics;	  // main statistics collector
	ThreadColorMap m_threadsColorLegend;
	ThreadTaskTypeMap m_currentThreadTaskType;
	ISystem* m_pSystem;

	int m_nHeightOffset;  // offset from bottom of the screen, in pixels

	DiskOperationInfo m_outStatistics;

	CDiskProfilerWindowsSpecific* m_windowsSpecificProfiling = nullptr;

public:
	static int profile_disk;
	static int profile_disk_max_items;
	static int profile_disk_max_draw_items;
	static float profile_disk_timeframe;  // max time for profiling timeframe
	static int profile_disk_type_filter;  // filter by task types for profiling
	static int profile_disk_budget;		  // filter by task types for profiling
};

#endif

#endif  // __diskprofilesystem_h__
