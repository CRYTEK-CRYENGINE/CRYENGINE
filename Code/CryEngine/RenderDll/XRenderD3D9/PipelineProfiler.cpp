// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PipelineProfiler.h"
#include "DriverD3D.h"
#include <Common/RenderDisplayContext.h>

CRenderPipelineProfiler::CRenderPipelineProfiler()
{
	m_frameDataIndex = 0;
	m_frameDataRT = m_frameData + m_frameDataIndex + 0;
	m_frameDataLRU = m_frameData + m_frameDataIndex + 1;
	m_avgFrameTime = 0;
	m_enabled = false;
	m_recordData = false;

	m_stack.reserve(8);

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
	{
		ResetBasicStats(m_basicStats[i], true);
		ResetDetailedStats(m_detailedStats[i], true);
	}
}

void CRenderPipelineProfiler::Init()
{
	for (uint32 i = 0; i < kNumPendingFrames; i++)
	{
		m_frameData[i].m_timestampGroup.Init();
	}
}

void CRenderPipelineProfiler::BeginFrame(const int frameID)
{
	m_recordData = IsEnabled();

	if (gEnv->IsEditor() && !gcpRendD3D->IsCurrentContextMainVP())
		m_recordData = false;

	if (!m_recordData)
		return;

	m_frameDataIndex = (m_frameDataIndex + 1) % kNumPendingFrames;
	m_frameDataRT = &m_frameData[m_frameDataIndex];

	SFrameData& frameData = *m_frameDataRT;
	memset(frameData.m_sections, 0, sizeof(frameData.m_sections));

	frameData.m_updated = false;
	frameData.m_numSections = 0;
	frameData.m_frameID = frameID;
	frameData.m_timestampGroup.BeginMeasurement();

	{
		RPProfilerStats* pBasicStats = m_basicStats[gRenDev->GetRenderThreadID()];
		DynArray<RPProfilerDetailedStats> &detailedStats = m_detailedStats[gRenDev->GetRenderThreadID()];

		ResetBasicStats(pBasicStats, true);
		ResetDetailedStats(detailedStats, true);

		detailedStats.resize(0);
	}

	// Open head
	{
		CRY_ASSERT(m_stack.empty());
		m_stack.resize(0);

		InsertSection("FRAME", eProfileSectionFlags_RootElement);
	}
}

void CRenderPipelineProfiler::EndFrame()
{
	if (!m_recordData)
		return;

	// Close head
	{
		EndSection("FRAME");

		CRY_ASSERT(m_stack.empty());
		m_stack.resize(0);
	}

	// Backup current CPU and GPU counters
	{
		// All the current frame's information has to be stored and associated with the current index
		// Otherwise global statistics and detailed statistics can be from entirely different frames (and utilization >100%)
		const uint32 fillThreadID = (uint32)gRenDev->GetMainThreadID();
		SFrameData& frameData = *m_frameDataRT;

		frameData.fTimeRealFrameTime  = iTimer->GetRealFrameTime();
		frameData.fTimeWaitForMain    = gcpRendD3D->m_fTimeWaitForMain   [fillThreadID];
		frameData.fTimeWaitForRender  = gcpRendD3D->m_fTimeWaitForRender [fillThreadID];
		frameData.fTimeProcessedRT    = gcpRendD3D->m_fTimeProcessedRT   [fillThreadID];
		frameData.fTimeProcessedGPU   = gcpRendD3D->m_fTimeProcessedGPU  [fillThreadID];
		frameData.fTimeWaitForGPU     = gcpRendD3D->m_fTimeWaitForGPU    [fillThreadID];
		frameData.fTimeGPUIdlePercent = gcpRendD3D->m_fTimeGPUIdlePercent[fillThreadID];

		frameData.m_timestampGroup.EndMeasurement();
	}

	// Get newest timestamps completed on gpu
	int prevFrameIndex = -1;
	for (int i = 1; i < kNumPendingFrames; ++i)
	{
		int tryFrameIndex = (m_frameDataIndex + (kNumPendingFrames - i)) % kNumPendingFrames;

		CRY_ASSERT((m_frameData + tryFrameIndex) != m_frameDataRT);
		if (m_frameData[tryFrameIndex].m_timestampGroup.ResolveTimestamps())
		{
			prevFrameIndex = tryFrameIndex;
			break;
		}
	}

	// The very first frame may not have any accessible measures
	if (prevFrameIndex >= 0)
	{
		CRY_ASSERT(m_frameData[prevFrameIndex].m_frameID != 0);

		UpdateSectionTimesAndStats(prevFrameIndex);
		UpdateStats(prevFrameIndex);
		UpdateThreadTimings(prevFrameIndex);

		// Display UI
		if (CRenderer::CV_r_profiler == 1)
			DisplayOverviewStats(prevFrameIndex);
		if (CRenderer::CV_r_profiler == 2)
			DisplayDetailedPassStats(prevFrameIndex);

		m_frameDataLRU = m_frameData + prevFrameIndex;
	}

	m_recordData = false;
}

bool CRenderPipelineProfiler::FilterLabel(const char* name)
{
	return
		(strcmp(name, "UpdateTextureRegion") == 0) ||
		(strcmp(name, "SCREEN_STRETCH_RECT") == 0) ||
		(strcmp(name, "STRETCHRECT") == 0) ||
		(strcmp(name, "STENCIL_VOLUME") == 0) ||
		(strcmp(name, "DRAWSTRINGW") == 0) ||
	  (strcmp(name, "DRAWSTRINGU") == 0);
}

#define SECTION_SKIP	~0U

uint32 CRenderPipelineProfiler::InsertSection(const char* name, uint32 profileSectionFlags)
{
	CRY_ASSERT(!m_recordData || (profileSectionFlags & eProfileSectionFlags_RootElement) || (m_stack.size() > 0));

	// Multi-threaded sections receive SKIP-identifier when section is filtered
	if (!m_recordData || FilterLabel(name))
		return SECTION_SKIP;

	// The stack is filled with SKIP-sections on overflow, to allow graceful nested section termination
	SFrameData& frameData = *m_frameDataRT;
	if (frameData.m_numSections >= SFrameData::kMaxNumSections)
	{
		m_stack.push_back(SECTION_SKIP);
		return SECTION_SKIP;
	}

	const uint32 sectionPos = frameData.m_numSections++;
	SProfilerSection& section = frameData.m_sections[sectionPos];

	cry_strcpy(section.name, name);

	section.flags = profileSectionFlags;
	section.recLevel = static_cast<int8>(m_stack.size() + 1);
	section.numDIPs = 0;
	section.numPolys = 0;

	if (!(profileSectionFlags & eProfileSectionFlags_MultithreadedSection))
	{
#if defined(ENABLE_PROFILING_CODE)
		section.numDIPs = SRenderStatistics::Write().GetNumberOfDrawCalls();
		section.numPolys = SRenderStatistics::Write().GetNumberOfPolygons();
#endif
		section.startTimeCPU = gEnv->pTimer->GetAsyncTime();
		section.startTimestamp = frameData.m_timestampGroup.IssueTimestamp(nullptr);
	}
	else
	{
		// Note: Stats from multi-threaded sections need to be hierarchically injected, they get handled later
	#if defined(ENABLE_PROFILING_CODE)
		section.numDIPs = 0;
		section.numPolys = 0;
	#endif
		section.startTimeCPU.SetValue(0);
		section.endTimeCPU.SetValue(0);
		section.startTimestamp = ~0u;
		section.endTimestamp = ~0u;
	}

	m_stack.push_back(sectionPos);

	string path = "";
	for (int i = 0; i < m_stack.size(); i++)
	{
		path += "\\";
		path += frameData.m_sections[m_stack[i]].name;
	}
	section.path = CCryNameTSCRC(path);

	return sectionPos;
}

void CRenderPipelineProfiler::BeginSection(const char* name)
{
	InsertSection(name, 0);
}

void CRenderPipelineProfiler::EndSection(const char* name)
{
	if (!m_recordData || FilterLabel(name))
		return;

	if (!m_stack.empty())
	{
		uint32 sectionPos = m_stack.back();
		m_stack.pop_back();

		if (sectionPos == SECTION_SKIP)
			return;

		SFrameData& frameData = *m_frameDataRT;
		SProfilerSection& section = frameData.m_sections[sectionPos];

		// In case a section is ended wrongly, mark the whole section as invalid (negative recLevel)
		if (strncmp(section.name, name, 30) != 0)
			section.recLevel = -section.recLevel;

		if (!(section.flags & eProfileSectionFlags_MultithreadedSection))
		{
#if defined(ENABLE_PROFILING_CODE)
			section.numDIPs = SRenderStatistics::Write().GetNumberOfDrawCalls() - section.numDIPs;
			section.numPolys = SRenderStatistics::Write().GetNumberOfPolygons() - section.numPolys;
#endif

			section.endTimeCPU = gEnv->pTimer->GetAsyncTime();
			section.endTimestamp = frameData.m_timestampGroup.IssueTimestamp(nullptr);
		}
	}
}

uint32 CRenderPipelineProfiler::InsertMultithreadedSection(const char* name)
{
	uint32 index = InsertSection(name, eProfileSectionFlags_MultithreadedSection);
	EndSection(name);
	return index;
}

void CRenderPipelineProfiler::UpdateMultithreadedSection(uint32 index, bool bSectionStart, int numDIPs, int numPolys, bool bIssueTimestamp, CTimeValue deltaTimestamp, CDeviceCommandList* pCommandList)
{
	if (!m_recordData || (index == SECTION_SKIP))
		return;

	{
		SFrameData& frameData = *m_frameDataRT;
		SProfilerSection& section = frameData.m_sections[index];

#if defined(ENABLE_PROFILING_CODE)
		CryInterlockedAdd(&section.numDIPs, numDIPs);
		CryInterlockedAdd(&section.numPolys, numPolys);
#endif

		section.endTimeCPU.AddValueThreadSafe(deltaTimestamp.GetValue());

		if (bIssueTimestamp)
		{
			static CryCriticalSection s_lock;
			AUTO_LOCK(s_lock);

			if (bSectionStart)
				section.startTimestamp = frameData.m_timestampGroup.IssueTimestamp(pCommandList);
			else
				section.endTimestamp = frameData.m_timestampGroup.IssueTimestamp(pCommandList);
		}
	}
}

void CRenderPipelineProfiler::UpdateSectionTimesAndStats(uint32 frameDataIndex)
{
	SFrameData& frameData = m_frameData[frameDataIndex];
	CRY_ASSERT(&frameData != m_frameDataRT);

	// This has been updated previously (frame-stats is repeated because of GPU delay)
	if (frameData.m_updated)
		return;

	for (uint32 i = 0, n = frameData.m_numSections; i < n; ++i)
	{
		SProfilerSection& section = frameData.m_sections[i];

		if (section.startTimestamp != ~0u && section.endTimestamp != ~0u)
			section.gpuTime = frameData.m_timestampGroup.GetTimeMS(section.startTimestamp, section.endTimestamp);
		else
			section.gpuTime = 0.0f;

		section.cpuTime = section.endTimeCPU.GetDifferenceInSeconds(section.startTimeCPU) * 1000.0f;
	}

	// Propagate values in multi-threaded sections up the hierarchy
	int drawcallSum[16] = { 0 };
	int polygonSum[16] = { 0 };
	int curRecLevel = frameData.m_numSections > 0 ? frameData.m_sections[frameData.m_numSections - 1].recLevel : 0;

	for (int32 i = frameData.m_numSections - 1; i >= 0; --i)
	{
		SProfilerSection& section = frameData.m_sections[i];

		if (section.recLevel >= static_cast<int8>(CRY_ARRAY_COUNT(drawcallSum)))
		{
			assert(0);
			continue;
		}

		if (section.recLevel < curRecLevel)
		{
			for (uint32 j = curRecLevel; j < CRY_ARRAY_COUNT(drawcallSum); j++)
			{
				section.numDIPs += drawcallSum[j];
				section.numPolys += polygonSum[j];
				drawcallSum[j] = 0;
				polygonSum[j] = 0;
			}

			drawcallSum[section.recLevel] += section.numDIPs;
			polygonSum[section.recLevel] += section.numPolys;
		}

		if (section.flags & eProfileSectionFlags_MultithreadedSection)
		{
			drawcallSum[section.recLevel] += section.numDIPs;
			polygonSum[section.recLevel] += section.numPolys;
		}

		curRecLevel = section.recLevel;
	}

	frameData.m_updated = true;
}

void CRenderPipelineProfiler::UpdateThreadTimings(uint32 frameDataIndex)
{
	SFrameData& frameData = m_frameData[frameDataIndex];
	CRY_ASSERT(&frameData != m_frameDataRT);

	// Simple Exponential Smoothing Weight
	// (1-a) * oldVal  + a * newVal
	// Range of "a": [0.0,1.0]
	const float smoothWeightDataOld = 1.f - CRenderer::CV_r_profilerSmoothingWeight;
	const float smoothWeightDataNew = CRenderer::CV_r_profilerSmoothingWeight;

	SThreadTimings& currThreadTimings = m_frameTimings[gRenDev->GetRenderThreadID()];
	SThreadTimings& prevThreadTimings = m_frameTimings[gRenDev->GetMainThreadID()];

	currThreadTimings.waitForMain    = (frameData.fTimeWaitForMain    * smoothWeightDataOld + prevThreadTimings.waitForMain   * smoothWeightDataNew);
	currThreadTimings.waitForRender  = (frameData.fTimeWaitForRender  * smoothWeightDataOld + prevThreadTimings.waitForRender * smoothWeightDataNew);
	currThreadTimings.waitForGPU     = (frameData.fTimeWaitForGPU     * smoothWeightDataOld + prevThreadTimings.waitForGPU    * smoothWeightDataNew);
	currThreadTimings.gpuIdlePerc    = (frameData.fTimeGPUIdlePercent * smoothWeightDataOld + prevThreadTimings.gpuIdlePerc   * smoothWeightDataNew);
	currThreadTimings.gpuFrameTime   = (frameData.fTimeProcessedGPU   * smoothWeightDataOld + prevThreadTimings.gpuFrameTime  * smoothWeightDataNew);
	currThreadTimings.frameTime      = (frameData.fTimeRealFrameTime  * smoothWeightDataOld + prevThreadTimings.frameTime     * smoothWeightDataNew);
	currThreadTimings.renderTime = min((frameData.fTimeProcessedRT    * smoothWeightDataOld + prevThreadTimings.renderTime    * smoothWeightDataNew), prevThreadTimings.frameTime);
}

void CRenderPipelineProfiler::ResetBasicStats(RPProfilerStats* pBasicStats, bool bResetAveragedStats)
{
	for (uint32 i = 0; i < RPPSTATS_NUM; ++i)
	{
		RPProfilerStats& basicStat = pBasicStats[i];
		basicStat.gpuTime = 0.0f;
		basicStat.cpuTime = 0.0f;
		basicStat.numDIPs = 0;
		basicStat.numPolys = 0;
	}

	if (bResetAveragedStats)
	{
		for (uint32 i = 0; i < RPPSTATS_NUM; ++i)
		{
			RPProfilerStats& basicStat = pBasicStats[i];
			basicStat.gpuTimeSmoothed = 0.0f;
			basicStat.gpuTimeMax = 0.0f;
			basicStat._gpuTimeMaxNew = 0.0f;
		}
	}
}

void CRenderPipelineProfiler::ResetDetailedStats(DynArray<RPProfilerDetailedStats>& detailedStats, bool bResetAveragedStats)
{
	for (RPProfilerDetailedStats& stat : detailedStats)
	{
		stat.cpuTime = 0.0f;
		stat.gpuTime = 0.0f;
		stat.startTimeCPU = 0.0f;
		stat.endTimeCPU = 0.0f;
		stat.startTimeGPU = 0;
		stat.endTimeGPU = 0;
		stat.flags = 0;
		stat.recLevel = 0;
		stat.numDIPs = 0;
		stat.numPolys = 0;
		memset(stat.name, 0, 31);

		if (bResetAveragedStats)
		{
			stat.gpuTimeSmoothed = 0.0f;
			stat.cpuTimeSmoothed = 0.0f;
		}
	}
}

CRenderPipelineProfiler::SProfilerSection& CRenderPipelineProfiler::FindSection(SFrameData& frameData, SProfilerSection& section)
{
	for (uint32 i = 0, n = frameData.m_numSections; i < n; ++i)
	{
		SProfilerSection& candidateSection = frameData.m_sections[i];
		if (candidateSection.path == section.path)
			return candidateSection;
	}

	return section;
}

void CRenderPipelineProfiler::ComputeAverageStats(SFrameData& currData, SFrameData& prevData)
{
	static int s_frameCounter = 0;
	const int kUpdateFrequency = 60;

	const uint32 processThreadID = (uint32)gRenDev->GetRenderThreadID();
	const uint32 fillThreadID = (uint32)gRenDev->GetMainThreadID();

	// Simple Exponential Smoothing Weight
	// (1-a) * oldVal  + a * newVal
	// Range of "a": [0.0,1.0]
	const float smoothWeightDataOld = 1.f - CRenderer::CV_r_profilerSmoothingWeight;
	const float smoothWeightDataNew = CRenderer::CV_r_profilerSmoothingWeight;

	// GPU times
	for (uint32 i = 0; i < RPPSTATS_NUM; ++i)
	{
		// Past data can be looked up
		RPProfilerStats& currentStat = m_basicStats[processThreadID][i];
		RPProfilerStats& prviousStat = m_basicStats[fillThreadID][i];

		// If no temporal history is found, set current data as history
		if (!prviousStat.gpuTimeSmoothed)
			prviousStat.gpuTimeSmoothed = prviousStat.gpuTime;
		if (!prviousStat._gpuTimeMaxNew )
			prviousStat._gpuTimeMaxNew  = prviousStat.gpuTime;

		currentStat.gpuTimeSmoothed =
			smoothWeightDataOld * prviousStat.gpuTimeSmoothed +
			smoothWeightDataNew * currentStat.gpuTime;

		float gpuTimeMax = std::max(currentStat._gpuTimeMaxNew, prviousStat._gpuTimeMaxNew);
		currentStat._gpuTimeMaxNew = std::max(gpuTimeMax, currentStat.gpuTime);

		if (s_frameCounter % kUpdateFrequency == 0)
		{
			currentStat.gpuTimeMax = currentStat._gpuTimeMaxNew;
			currentStat._gpuTimeMaxNew = 0;
		}
	}

	for (uint32 i = 0, n = currData.m_numSections; i < n; ++i)
	{
		// Past data can be searched in the previous frame's data
		SProfilerSection& currentSection = currData.m_sections[i];
		SProfilerSection& prviousSection = FindSection(prevData, currentSection);

		// If no temporal history is found, set current data as history
		if (!prviousSection.gpuTimeSmoothed)
			prviousSection.gpuTimeSmoothed = prviousSection.gpuTime;
		if (!prviousSection.cpuTimeSmoothed)
			prviousSection.cpuTimeSmoothed = prviousSection.cpuTime;

		currentSection.gpuTimeSmoothed =
			smoothWeightDataOld * prviousSection.gpuTimeSmoothed +
			smoothWeightDataNew * currentSection.gpuTime;
		currentSection.cpuTimeSmoothed =
			smoothWeightDataOld * prviousSection.cpuTimeSmoothed +
			smoothWeightDataNew * currentSection.cpuTime;

		m_detailedStats[processThreadID][i].gpuTime         = currentSection.gpuTime;
		m_detailedStats[processThreadID][i].cpuTime         = currentSection.cpuTime;
		m_detailedStats[processThreadID][i].gpuTimeSmoothed = currentSection.gpuTimeSmoothed;
		m_detailedStats[processThreadID][i].cpuTimeSmoothed = currentSection.cpuTimeSmoothed;
	}

	s_frameCounter += 1;
}

ILINE void CRenderPipelineProfiler::AddToStats(RPProfilerStats& outStats, SProfilerSection& section)
{
	outStats.gpuTime += section.gpuTime;
	outStats.cpuTime += section.cpuTime;
	outStats.numDIPs += section.numDIPs;
	outStats.numPolys += section.numPolys;
}

ILINE void CRenderPipelineProfiler::SubtractFromStats(RPProfilerStats& outStats, SProfilerSection& section)
{
	outStats.gpuTime -= section.gpuTime;
	outStats.cpuTime -= section.cpuTime;
	outStats.numDIPs -= section.numDIPs;
	outStats.numPolys -= section.numPolys;
}

void CRenderPipelineProfiler::UpdateStats(uint32 frameDataIndex)
{
	RPProfilerStats* pBasicStats = m_basicStats[gRenDev->GetRenderThreadID()];
	DynArray<RPProfilerDetailedStats> &detailedStats = m_detailedStats[gRenDev->GetRenderThreadID()];

	bool bRecursivePass = false;
	SFrameData& frameData = m_frameData[frameDataIndex];
	CRY_ASSERT(&frameData != m_frameDataRT);

	detailedStats.resize(frameData.m_numSections);
	for (uint32 i = 0, n = frameData.m_numSections; i < n; ++i)
	{
		SProfilerSection& section = frameData.m_sections[i];

		if (strcmp(section.name, "SCENE_REC") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_Recursion], section);
			bRecursivePass = true;
		}
		else if (strcmp(section.name, "SCENE") == 0)
		{
			bRecursivePass = false;
		}

		if (bRecursivePass)
			continue;

		// Scene
		if (strcmp(section.name, "GBUFFER") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_SceneOverall], section);
		}
		else if (strcmp(section.name, "DECALS") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_SceneDecals], section);
		}
		else if (strcmp(section.name, "DEFERRED_DECALS") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_SceneOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_SceneDecals], section);
		}
		else if (strcmp(section.name, "OPAQUE_PASSES") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_SceneOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_SceneForward], section);
		}
		else if (strcmp(section.name, "WATER") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_SceneOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_SceneWater], section);
		}
		// Shadows
		else if (strstr(section.name, "SUN PER OBJECT ") == section.name)
		{
			AddToStats(pBasicStats[eRPPSTATS_ShadowsOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_ShadowsSunCustom], section);
		}
		else if (strstr(section.name, "SUN ") == section.name)
		{
			AddToStats(pBasicStats[eRPPSTATS_ShadowsOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_ShadowsSun], section);
		}
		else if (strstr(section.name, "LOCAL LIGHT ") == section.name)
		{
			AddToStats(pBasicStats[eRPPSTATS_ShadowsOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_ShadowsLocal], section);
		}
		// Lighting
		else if (strcmp(section.name, "TILED_SHADING") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_LightingOverall], section);
		}
		else if (strcmp(section.name, "DEFERRED_SHADING") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_LightingOverall], section);
		}
		else if (strcmp(section.name, "DEFERRED_CUBEMAPS") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_LightingOverall], section);
		}
		else if (strcmp(section.name, "DEFERRED_LIGHTS") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_LightingOverall], section);
		}
		else if (strcmp(section.name, "AMBIENT_PASS") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_LightingOverall], section);
		}
		else if (strcmp(section.name, "SVOGI") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_LightingOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_LightingGI], section);
		}
		// VFX
		else if (strcmp(section.name, "TRANSPARENT_BW") == 0 || strcmp(section.name, "TRANSPARENT_AW") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_VfxOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_VfxTransparent], section);
		}
		else if (strcmp(section.name, "FOG_GLOBAL") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_VfxOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_VfxFog], section);
		}
		else if (strcmp(section.name, "VOLUMETRIC FOG") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_VfxOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_VfxFog], section);
		}
		else if (strcmp(section.name, "DEFERRED_RAIN") == 0 || strcmp(section.name, "RAIN") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_VfxOverall], section);
		}
		else if (strcmp(section.name, "LENS_OPTICS") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_VfxOverall], section);
			AddToStats(pBasicStats[eRPPSTATS_VfxFlares], section);
		}
		else if (strcmp(section.name, "OCEAN CAUSTICS") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_VfxOverall], section);
		}
		else if (strcmp(section.name, "WATERVOLUME_CAUSTICS") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_VfxOverall], section);
		}
		else if (strcmp(section.name, "GPU_PARTICLES") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_VfxOverall], section);
		}
		// Extra TI states
		else if (strcmp(section.name, "TI_INJECT_CLEAR") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_INJECT_CLEAR], section);
		}
		else if (strcmp(section.name, "TI_VOXELIZE") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_VOXELIZE], section);
		}
		else if (strcmp(section.name, "TI_INJECT_LIGHT") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_INJECT_LIGHT], section);
		}
		else if (strcmp(section.name, "TI_INJECT_AIR") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_INJECT_AIR], section);
		}
		else if (strcmp(section.name, "TI_INJECT_REFL0") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_INJECT_REFL0], section);
		}
		else if (strcmp(section.name, "TI_INJECT_REFL1") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_INJECT_REFL1], section);
		}
		else if (strcmp(section.name, "TI_INJECT_DYNL") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_INJECT_DYNL], section);
		}
		else if (strcmp(section.name, "TI_NID_DIFF") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_NID_DIFF], section);
		}
		else if (strcmp(section.name, "TI_GEN_DIFF") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_GEN_DIFF], section);
		}
		else if (strcmp(section.name, "TI_GEN_SPEC") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_GEN_SPEC], section);
		}
		else if (strcmp(section.name, "TI_GEN_AIR") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_GEN_AIR], section);
		}
		else if (strcmp(section.name, "TI_UPSCALE_DIFF") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_UPSCALE_DIFF], section);
		}
		else if (strcmp(section.name, "TI_UPSCALE_SPEC") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_UPSCALE_SPEC], section);
		}
		else if (strcmp(section.name, "TI_DEMOSAIC_DIFF") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_DEMOSAIC_DIFF], section);
		}
		else if (strcmp(section.name, "TI_DEMOSAIC_SPEC") == 0)
		{
			AddToStats(pBasicStats[eRPPSTATS_TI_DEMOSAIC_SPEC], section);
		}

		// Update detailed stats
		memcpy(detailedStats[i].name, section.name, 31);
		detailedStats[i].cpuTime = section.cpuTime;
		detailedStats[i].gpuTime = section.gpuTime;
		detailedStats[i].cpuTimeSmoothed = section.cpuTimeSmoothed;
		detailedStats[i].gpuTimeSmoothed = section.gpuTimeSmoothed;
		detailedStats[i].startTimeCPU = section.startTimeCPU;
		detailedStats[i].endTimeCPU = section.endTimeCPU;
		detailedStats[i].startTimeGPU = section.startTimestamp!= ~0u ? frameData.m_timestampGroup.GetTime(section.startTimestamp) : 0;
		detailedStats[i].endTimeGPU = section.endTimestamp != ~0u ? frameData.m_timestampGroup.GetTime(section.endTimestamp) : 0;
		detailedStats[i].numDIPs = section.numDIPs;
		detailedStats[i].numPolys = section.numPolys;
		detailedStats[i].recLevel = section.recLevel;
		detailedStats[i].flags = section.flags;
	}

	AddToStats(pBasicStats[eRPPSTATS_OverallFrame], frameData.m_sections[0]);

	ComputeAverageStats(frameData, *m_frameDataLRU);
}

void CRenderPipelineProfiler::DisplayDetailedPassStats(uint32 frameDataIndex)
{
#ifndef _RELEASE
	if (gEnv->pConsole->IsOpened())
		return;

	CD3D9Renderer* rd = gcpRendD3D;
	CRenderDisplayContext* pDC = rd->GetActiveDisplayContext();
	SFrameData& frameData = m_frameData[frameDataIndex];
	CRY_ASSERT(&frameData != m_frameDataRT);
	uint32 elemsPerColumn = (pDC->GetDisplayResolution()[1] - 60) / 16;

	// TODO: relative/normalized coordinate system in screen-space
	float sx = /*VIRTUAL_SCREEN_WIDTH */ float(pDC->GetDisplayResolution()[0]);
	float sy = /*VIRTUAL_SCREEN_HEIGHT*/ float(pDC->GetDisplayResolution()[1]);

	// Dim background to make text more readable
	IRenderAuxImage::Draw2dImage(0, 0, sx, sy, CRendererResources::s_ptexWhite->GetID(), 0, 0, 1, 1, 0, ColorF(0.05f, 0.05f, 0.05f, 0.5f));

	ColorF color = frameData.m_numSections >= SFrameData::kMaxNumSections ? Col_Red : ColorF(1.0f, 1.0f, 0.2f, 1);
	m_avgFrameTime = 0.8f * frameData.fTimeRealFrameTime + 0.2f * m_avgFrameTime; // exponential moving average for frame time

	int frameDist = m_frameDataIndex - frameDataIndex;
	frameDist += frameDist < 0 ? kNumPendingFrames : 0;
	color = ColorF(0.35f, 0.35f, 0.35f);
	IRenderAuxText::Draw2dLabel(20, 10, 1.7f, &color.r, false, "Showing data for frame n-%i", frameDist);
	IRenderAuxText::Draw2dLabel(320, 10, 1.5f, &color.r, false, "GPU");
	IRenderAuxText::Draw2dLabel(400, 10, 1.5f, &color.r, false, "CPU");
	IRenderAuxText::Draw2dLabel(470, 10, 1.5f, &color.r, false, "DIPs");
	IRenderAuxText::Draw2dLabel(520, 10, 1.5f, &color.r, false, "Polys");

	if (frameData.m_numSections > elemsPerColumn)
	{
		IRenderAuxText::Draw2dLabel(320 + 600, 10, 1.5f, &color.r, false, "GPU");
		IRenderAuxText::Draw2dLabel(400 + 600, 10, 1.5f, &color.r, false, "CPU");
		IRenderAuxText::Draw2dLabel(470 + 600, 10, 1.5f, &color.r, false, "DIPs");
		IRenderAuxText::Draw2dLabel(520 + 600, 10, 1.5f, &color.r, false, "Polys");
	}

	// Refresh the list every 3 seconds to clear out old data and reduce gaps
	CTimeValue currentTime = gEnv->pTimer->GetAsyncTime();
	static CTimeValue lastClearTime = currentTime;

	if (currentTime.GetDifferenceInSeconds(lastClearTime) > 3.0f)
	{
		lastClearTime = currentTime;
		m_staticNameList.clear();
	}

	// Reset the used flag
	for (std::multimap<CCryNameTSCRC, SStaticElementInfo>::iterator it = m_staticNameList.begin(); it != m_staticNameList.end(); ++it)
	{
		it->second.bUsed = false;
	}

	// Find median of GPU times
	float medianTimeGPU = 0;
	if (frameData.m_numSections > 0)
	{
		static std::vector<float> s_arrayTimes;
		s_arrayTimes.resize(0);
		for (uint32 i = 0, n = frameData.m_numSections; i < n; ++i)
		{
			s_arrayTimes.push_back(frameData.m_sections[i].gpuTime);
		}
		std::sort(s_arrayTimes.begin(), s_arrayTimes.end());
		medianTimeGPU = s_arrayTimes[frameData.m_numSections / 2];
	}

	SThreadTimings& currThreadTimings = m_frameTimings[gRenDev->GetRenderThreadID()];
	float frameTimeGPU = max(currThreadTimings.gpuFrameTime * 1000.0f, 0.0f);

	for (uint32 i = 0, n = frameData.m_numSections; i < n; ++i)
	{
		SProfilerSection& section = frameData.m_sections[i];

		// find the slot we should render to according to the static list - if not found, insert
		std::multimap<CCryNameTSCRC, SStaticElementInfo>::iterator it = m_staticNameList.lower_bound(section.path);
		while (it != m_staticNameList.end() && it->second.bUsed)
			++it;

		// not found: either a new element or a duplicate - insert at correct position and render there
		if (it == m_staticNameList.end() || it->first != section.path)
			it = m_staticNameList.insert(std::make_pair(section.path, i));

		it->second.bUsed = true;

		float gpuTimeSmoothed = section.gpuTimeSmoothed;
		float cpuTimeSmoothed = section.cpuTimeSmoothed;
		float ypos = 30.0f + (it->second.nPos % elemsPerColumn) * 16.0f;
		float xpos = 20.0f + ((int)(it->second.nPos / elemsPerColumn)) * 600.0f;

		if (section.recLevel < 0)   // Label stack error
		{
			color = ColorF(1, 0, 0);
		}
		else
		{
			// Highlight items which are more expensive relative to the other items in the list
			color.r = color.g = color.b = 0.4f + 0.3f * std::min(gpuTimeSmoothed / medianTimeGPU, 2.0f);
			// Tint items which are expensive relative to the overall frame time
			color.b *= clamp_tpl(1.2f - (gpuTimeSmoothed / frameTimeGPU) * 8.0f, 0.0f, 1.0f);
		}

		IRenderAuxText::Draw2dLabel(xpos + max((int)(abs(section.recLevel) - 2), 0) * 15.0f, ypos, 1.5f, &color.r, false, "%s", section.name);
		IRenderAuxText::Draw2dLabel(xpos + 300, ypos, 1.5f, &color.r, false, "%.2fms", gpuTimeSmoothed);
		IRenderAuxText::Draw2dLabel(xpos + 380, ypos, 1.5f, &color.r, false, "%.2fm%c", cpuTimeSmoothed, !(section.flags & eProfileSectionFlags_MultithreadedSection) ? 's' : 't');
		IRenderAuxText::Draw2dLabel(xpos + 450, ypos, 1.5f, &color.r, false, "%i", section.numDIPs);
		IRenderAuxText::Draw2dLabel(xpos + 500, ypos, 1.5f, &color.r, false, "%i", section.numPolys);
	}
#endif
}

namespace DebugUI
{
const float columnHeight = 0.027f;

void DrawText(float x, float y, float size, ColorF color, const char* text)
{
	// TODO: relative/normalized coordinate system in screen-space
	CRenderDisplayContext* pDC = gcpRendD3D->GetActiveDisplayContext();
	float aspect = pDC->GetAspectRatio();
	float sx = VIRTUAL_SCREEN_WIDTH  /*float(pDC->GetDisplayResolution()[0]) */ / aspect;
	float sy = VIRTUAL_SCREEN_HEIGHT /*float(pDC->GetDisplayResolution()[1])*/;

	IRenderAuxText::DrawText(Vec3(x * sx, y * sy, 1.f), IRenderAuxText::ASize(size * 1.55f / aspect, size * 1.1f), color, eDrawText_800x600 | eDrawText_2D | eDrawText_LegacyBehavior, text);
}

void DrawText(float x, float y, float size, ColorF color, const char* format, va_list args)
{
	char buffer[512];
	cry_vsprintf(buffer, format, args);

	DrawText(x, y, size, color, buffer);
}

void DrawBox(float x, float y, float width, float height, ColorF color)
{
	// TODO: relative/normalized coordinate system in screen-space
	CD3D9Renderer* rd = gcpRendD3D;
	CRenderDisplayContext* pDC = rd->GetActiveDisplayContext();
	float aspect = pDC->GetAspectRatio();
	float sx = /*VIRTUAL_SCREEN_WIDTH */ float(pDC->GetDisplayResolution()[0]) / aspect;
	float sy = /*VIRTUAL_SCREEN_HEIGHT*/ float(pDC->GetDisplayResolution()[1]);
//	const Vec2 overscanOffset = Vec2(rd->s_overscanBorders.x * VIRTUAL_SCREEN_WIDTH, rd->s_overscanBorders.y * VIRTUAL_SCREEN_HEIGHT);
	const Vec2 overscanOffset = Vec2(rd->s_overscanBorders.x * pDC->GetDisplayResolution()[0], rd->s_overscanBorders.y * pDC->GetDisplayResolution()[1]);
	IRenderAuxImage::Draw2dImage(x * sx + overscanOffset.x, y * sy + overscanOffset.y, width * sx, height * sy, CRendererResources::s_ptexWhite->GetID(), 0, 0, 1, 1, 0, color);
}

void DrawTable(float x, float y, float width, int numColumns, const char* title)
{
	DrawBox(x, y, width, columnHeight, ColorF(0.45f, 0.45f, 0.55f, 0.6f));
	DrawBox(x, y + columnHeight, width, columnHeight * (float)numColumns + 0.007f, ColorF(0.05f, 0.05f, 0.05f, 0.6f));
	DrawText(x + 0.006f, y + 0.004f, 1, Col_Salmon, title);
}

void DrawTableColumn(float tableX, float tableY, int columnIndex, const char* columnText, ...)
{
	va_list args;
	va_start(args, columnText);
	DrawText(tableX + 0.02f, tableY + (float)(columnIndex + 1) * columnHeight + 0.005f, 1, Col_White, columnText, args);
	va_end(args);
}

void DrawTableBar(float x, float tableY, int columnIndex, float percentage, ColorF color)
{
	const float barHeight = 0.02f;
	const float barWidth = 0.15f;

	DrawBox(x, tableY + (float)(columnIndex + 1) * columnHeight + (columnHeight - barHeight) * 0.5f + 0.005f,
	        barWidth, barHeight, ColorF(1, 1, 1, 0.2f));

	DrawBox(x, tableY + (float)(columnIndex + 1) * columnHeight + (columnHeight - barHeight) * 0.5f + 0.005f,
	        percentage * barWidth, barHeight, ColorF(color.r, color.g, color.b, 0.7f));
}
}

void CRenderPipelineProfiler::DisplayOverviewStats(uint32 frameDataIndex)
{
#ifndef _RELEASE
	if (gEnv->pConsole->IsOpened())
		return;

	struct StatsGroup
	{
		char                         name[32];
		ERenderPipelineProfilerStats statIndex;
	};

	StatsGroup statsGroups[] = {
		{ "Frame",               eRPPSTATS_OverallFrame     },
		{ "  Ocean Reflections", eRPPSTATS_Recursion        },
		{ "  Scene",             eRPPSTATS_SceneOverall     },
		{ "    Decals",          eRPPSTATS_SceneDecals      },
		{ "    Forward",         eRPPSTATS_SceneForward     },
		{ "    Water",           eRPPSTATS_SceneWater       },
		{ "  Shadows",           eRPPSTATS_ShadowsOverall   },
		{ "    Sun",             eRPPSTATS_ShadowsSun       },
		{ "    Per-Object",      eRPPSTATS_ShadowsSunCustom },
		{ "    Local",           eRPPSTATS_ShadowsLocal     },
		{ "  Lighting",          eRPPSTATS_LightingOverall  },
		{ "    Voxel GI",        eRPPSTATS_LightingGI       },
		{ "  VFX",               eRPPSTATS_VfxOverall       },
		{ "    Particles/Glass", eRPPSTATS_VfxTransparent   },
		{ "    Fog",             eRPPSTATS_VfxFog           },
		{ "    Flares",          eRPPSTATS_VfxFlares        },
	};

	// Threading info
	{
		SThreadTimings& currThreadTimings = m_frameTimings[gRenDev->GetRenderThreadID()];

		DebugUI::DrawTable(0.05f, 0.1f, 0.45f, 4, "Overview");

		float frameTime = currThreadTimings.frameTime;
		float mainThreadTime = max(currThreadTimings.frameTime - currThreadTimings.waitForRender, 0.0f);
		float renderThreadTime = max(currThreadTimings.renderTime - currThreadTimings.waitForGPU, 0.0f);
	#ifdef CRY_PLATFORM_ORBIS
		float gpuTime = max((100.0f - currThreadTimings.gpuIdlePerc) * frameTime * 0.01f, 0.0f);
	#else
		float gpuTime = max(currThreadTimings.gpuFrameTime, 0.0f);
	#endif
		float waitForGPU = max(currThreadTimings.waitForGPU, 0.0f);

		DebugUI::DrawTableBar(0.335f, 0.1f, 0, mainThreadTime / frameTime, Col_Yellow);
		DebugUI::DrawTableBar(0.335f, 0.1f, 1, renderThreadTime / frameTime, Col_Green);
		DebugUI::DrawTableBar(0.335f, 0.1f, 2, gpuTime / frameTime, Col_Cyan);
		DebugUI::DrawTableBar(0.335f, 0.1f, 3, waitForGPU / frameTime, Col_Red);

		DebugUI::DrawTableColumn(0.05f, 0.1f, 0, "Main Thread             %6.2f ms", mainThreadTime * 1000.0f);
		DebugUI::DrawTableColumn(0.05f, 0.1f, 1, "Render Thread           %6.2f ms", renderThreadTime * 1000.0f);
		DebugUI::DrawTableColumn(0.05f, 0.1f, 2, "GPU                     %6.2f ms", gpuTime * 1000.0f);
		DebugUI::DrawTableColumn(0.05f, 0.1f, 3, "CPU waits for GPU       %6.2f ms", waitForGPU * 1000.0f);
	}

	// GPU times
	{
		const float targetFrameTime = 1000.0f / CRenderer::CV_r_profilerTargetFPS;

		int numColumns = sizeof(statsGroups) / sizeof(StatsGroup);
		DebugUI::DrawTable(0.05f, 0.27f, 0.45f, numColumns, "GPU Time");

		RPProfilerStats* pBasicStats = m_basicStats[gRenDev->GetRenderThreadID()];
		for (uint32 i = 0; i < numColumns; ++i)
		{
			const RPProfilerStats& stats = pBasicStats[statsGroups[i].statIndex];
			DebugUI::DrawTableColumn(0.05f, 0.27f, i, "%-20s  %4.1f ms  %2.0f %%", statsGroups[i].name, stats.gpuTimeSmoothed, stats.gpuTimeSmoothed / targetFrameTime * 100.0f);
		}
	}
#endif
}

bool CRenderPipelineProfiler::IsEnabled()
{
	return m_enabled || CRenderer::CV_r_profiler;
}