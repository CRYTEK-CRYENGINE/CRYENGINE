// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#include "DeviceManager/DeviceObjects.h"

class CRenderPipelineProfiler
{
public:
	enum EProfileSectionFlags
	{
		eProfileSectionFlags_MultithreadedSection = BIT(1)
	};
	
	CRenderPipelineProfiler();

	void                   Init();
	void                   BeginFrame();
	void                   EndFrame();
	void                   BeginSection(const char* name);
	void                   EndSection(const char* name);

	uint32                 InsertMultithreadedSection(const char* name);
	void                   UpdateMultithreadedSection(uint32 index, bool bSectionStart, int numDIPs, int numPolys, bool bIssueGPUTimestamp, CDeviceCommandList* pCommandList);
	
	bool                   IsEnabled();
	void                   SetEnabled(bool enabled)                                        { m_enabled = enabled; }

	const RPProfilerStats& GetBasicStats(ERenderPipelineProfilerStats stat, int nThreadID) { assert((uint32)stat < RPPSTATS_NUM); return m_basicStats[nThreadID][stat]; }
	const RPProfilerStats* GetBasicStatsArray(int nThreadID)                               { return m_basicStats[nThreadID]; }

protected:
	struct SProfilerSection
	{
		char            name[31];
		int8            recLevel;   // Negative value means error in stack
		uint8           flags;
		CCryNameTSCRC   path;
		int             numDIPs, numPolys;
		CTimeValue      startTimeCPU, endTimeCPU;
		uint32          startTimestamp, endTimestamp;
		float           gpuTime;
	};

	struct SFrameData
	{
		enum { kMaxNumSections = CDeviceTimestampGroup::kMaxTimestamps / 2 };

		uint32                 m_numSections;
		SProfilerSection       m_sections[kMaxNumSections];
		CDeviceTimestampGroup  m_timestampGroup;

		// Note: Use of m_sections is guarded by m_numSections, initialization of m_sections skipped for performance reasons
		// cppcheck-suppress uninitMemberVar
		SFrameData()
			: m_numSections(0)
		{
		}
	};

	struct SThreadTimings
	{
		float waitForMain;
		float waitForRender;
		float waitForGPU;
		float gpuIdlePerc;
		float gpuFrameTime;
		float frameTime;
		float renderTime;

		SThreadTimings()
			: waitForMain(0)
			, waitForRender(0)
			, waitForGPU(0)
			, gpuIdlePerc(0)
			, gpuFrameTime(33.0f)
			, frameTime(33.0f)
			, renderTime(0)
		{
		}
	};

	struct SStaticElementInfo
	{
		uint nPos;
		bool bUsed;

		SStaticElementInfo(uint _nPos)
			: nPos(_nPos)
			, bUsed(false)
		{
		}
	};

protected:
	uint32 InsertSection(const char* name, uint32 profileSectionFlags = 0);
	
	bool FilterLabel(const char* name);
	void UpdateGPUTimes(uint32 frameDataIndex);
	void UpdateThreadTimings();
	
	void ResetBasicStats(RPProfilerStats* pBasicStats, bool bResetAveragedStats);
	void ComputeAverageStats();
	void AddToStats(RPProfilerStats& outStats, SProfilerSection& section);
	void SubtractFromStats(RPProfilerStats& outStats, SProfilerSection& section);
	void UpdateBasicStats(uint32 frameDataIndex);

	void DisplayOverviewStats();
	void DisplayDetailedPassStats(uint32 frameDataIndex);

protected:
	enum { kNumPendingFrames = MAX_FRAMES_IN_FLIGHT + 1 };

	std::vector<uint32> m_stack;
	SFrameData          m_frameData[kNumPendingFrames];
	uint32              m_frameDataIndex;
	float               m_avgFrameTime;
	bool                m_enabled;
	bool                m_recordData;

	RPProfilerStats     m_basicStats[RT_COMMAND_BUF_COUNT][RPPSTATS_NUM];
	SThreadTimings      m_threadTimings;

	// we take a snapshot every now and then and store it in here to prevent the text from jumping too much
	std::multimap<CCryNameTSCRC, SStaticElementInfo> m_staticNameList;
};
