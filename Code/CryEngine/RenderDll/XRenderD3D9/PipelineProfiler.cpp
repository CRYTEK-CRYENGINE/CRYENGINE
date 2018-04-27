// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PipelineProfiler.h"
#include <Common/RenderDisplayContext.h>

CRenderPipelineProfiler::CRenderPipelineProfiler()
{
	m_frameDataIndex = 0;
	m_avgFrameTime = 0;
	m_enabled = false;
	m_recordData = false;

	m_stack.reserve(8);

	for (int i = 0; i < RT_COMMAND_BUF_COUNT; i++)
		ResetBasicStats(m_basicStats[i], true);
}

void CRenderPipelineProfiler::Init()
{
	for (uint32 i = 0; i < kNumPendingFrames; i++)
	{
		m_frameData[i].m_timestampGroup.Init();
	}
}

void CRenderPipelineProfiler::BeginFrame()
{
	m_recordData = IsEnabled();

	if (gEnv->IsEditor() && !gcpRendD3D->IsCurrentContextMainVP())
		m_recordData = false;

	if (!m_recordData)
		return;

	m_frameDataIndex = (m_frameDataIndex + 1) % kNumPendingFrames;

	SFrameData& frameData = m_frameData[m_frameDataIndex];
	frameData.m_numSections = 0;

	frameData.m_timestampGroup.BeginMeasurement();

	BeginSection("FRAME");
	frameData.m_sections[0].numDIPs = 0;
	frameData.m_sections[0].numPolys = 0;
}

void CRenderPipelineProfiler::EndFrame()
{
	if (!m_recordData)
		return;

	EndSection("FRAME");

	SFrameData& frameData = m_frameData[m_frameDataIndex];

	if (!m_stack.empty())
	{
		frameData.m_sections[0].recLevel = -1;
		m_stack.resize(0);
	}

	frameData.m_timestampGroup.EndMeasurement();

	// Get newest timestamps completed on gpu
	int prevFrameIndex = -1;
	for (int i = 1; i < kNumPendingFrames; ++i)
	{
		prevFrameIndex = (m_frameDataIndex + (kNumPendingFrames - i)) % kNumPendingFrames;
		if (m_frameData[prevFrameIndex].m_timestampGroup.ResolveTimestamps())
			break;

	}

	CRY_ASSERT(prevFrameIndex != -1);
	
	if (prevFrameIndex >= 0)
	{
		UpdateGPUTimes(prevFrameIndex);
		UpdateBasicStats(prevFrameIndex);
		UpdateThreadTimings();

		m_recordData = false;

		// Display UI
		if (CRenderer::CV_r_profiler == 1)
			DisplayOverviewStats();
		if (CRenderer::CV_r_profiler == 2)
			DisplayDetailedPassStats(prevFrameIndex);
	}
}

bool CRenderPipelineProfiler::FilterLabel(const char* name)
{
	return
	  (strcmp(name, "SCREEN_STRETCH_RECT") == 0) ||
	  (strcmp(name, "STRETCHRECT") == 0) ||
	  (strcmp(name, "STENCIL_VOLUME") == 0) ||
	  (strcmp(name, "DRAWSTRINGW") == 0) ||
	  (strcmp(name, "DRAWSTRINGU") == 0);
}

uint32 CRenderPipelineProfiler::InsertSection(const char* name, uint32 profileSectionFlags)
{
	SFrameData& frameData = m_frameData[m_frameDataIndex];

	if (frameData.m_numSections >= SFrameData::kMaxNumSections)
		m_recordData = false;

	if (!m_recordData || FilterLabel(name))
		return ~0u;

	SProfilerSection& section = frameData.m_sections[frameData.m_numSections++];

	cry_strcpy(section.name, name);

	section.flags = profileSectionFlags;
	section.recLevel = static_cast<int8>(m_stack.size() + 1);
	section.numDIPs = 0;
	section.numPolys = 0;

	if (!(profileSectionFlags & eProfileSectionFlags_MultithreadedSection))
	{
#if defined(ENABLE_PROFILING_CODE)
		// Note: Stats from multithreaded sections need to be subtracted, they get handled later
		section.numDIPs = gcpRendD3D->GetCurrentNumberOfDrawCalls() - SRenderStatistics::Write().m_nScenePassDIPs;
		section.numPolys = gcpRendD3D->RT_GetPolyCount() - SRenderStatistics::Write().m_nScenePassPolygons;
#endif
		section.startTimeCPU = gEnv->pTimer->GetAsyncTime();
		section.startTimestamp = frameData.m_timestampGroup.IssueTimestamp(nullptr);
	}
	else
	{
	#if defined(ENABLE_PROFILING_CODE)
		section.numDIPs = 0;
		section.numPolys = 0;
	#endif
		section.startTimeCPU.SetValue(0);
		section.endTimeCPU.SetValue(0);
		section.startTimestamp = ~0u;
		section.endTimestamp = ~0u;
	}

	m_stack.push_back(frameData.m_numSections - 1);

	string path = "";
	for (int i = 0; i < m_stack.size(); i++)
	{
		path += "\\";
		path += frameData.m_sections[m_stack[i]].name;
	}
	section.path = CCryNameTSCRC(path);

	return frameData.m_numSections - 1;
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
		SFrameData& frameData = m_frameData[m_frameDataIndex];
		SProfilerSection& section = frameData.m_sections[m_stack.back()];

		if (strncmp(section.name, name, 30) != 0)
			section.recLevel = -section.recLevel;

		if (!(section.flags & eProfileSectionFlags_MultithreadedSection))
		{
#if defined(ENABLE_PROFILING_CODE)
			section.numDIPs = (gcpRendD3D->GetCurrentNumberOfDrawCalls() - SRenderStatistics::Write().m_nScenePassDIPs) - section.numDIPs;
			section.numPolys = (gcpRendD3D->RT_GetPolyCount() - SRenderStatistics::Write().m_nScenePassPolygons) - section.numPolys;
#endif
			section.endTimeCPU = gEnv->pTimer->GetAsyncTime();
			section.endTimestamp = frameData.m_timestampGroup.IssueTimestamp(nullptr);
		}

		m_stack.pop_back();
	}
}

uint32 CRenderPipelineProfiler::InsertMultithreadedSection(const char* name)
{
	uint32 index = InsertSection(name, eProfileSectionFlags_MultithreadedSection);
	EndSection(name);
	return index;
}

void CRenderPipelineProfiler::UpdateMultithreadedSection(uint32 index, bool bSectionStart, int numDIPs, int numPolys, bool bIssueGPUTimestamp, CDeviceCommandList* pCommandList)
{
	if (m_recordData && index != ~0u)
	{
		static CryCriticalSection s_lock;
		AUTO_LOCK(s_lock);

		SFrameData& frameData = m_frameData[m_frameDataIndex];
		SProfilerSection& section = frameData.m_sections[index];

		CryInterlockedAdd(&section.numDIPs, numDIPs);
		CryInterlockedAdd(&section.numPolys, numPolys);

		if (bIssueGPUTimestamp)
		{
			if (bSectionStart)
				section.startTimestamp = frameData.m_timestampGroup.IssueTimestamp(pCommandList);
			else
				section.endTimestamp = frameData.m_timestampGroup.IssueTimestamp(pCommandList);
		}
	}
}

void CRenderPipelineProfiler::UpdateGPUTimes(uint32 frameDataIndex)
{
	SFrameData& frameData = m_frameData[frameDataIndex];
	for (uint32 i = 0; i < frameData.m_numSections; ++i)
	{
		SProfilerSection& section = frameData.m_sections[i];

		if (section.startTimestamp != ~0u && section.endTimestamp != ~0u)
			section.gpuTime = frameData.m_timestampGroup.GetTimeMS(section.startTimestamp, section.endTimestamp);
		else
			section.gpuTime = 0.0f;
	}

	// Propagate values in multi-threaded sections up the hierarchy
	int drawcallSum[8] = { 0 };
	int polygonSum[8] = { 0 };
	int curRecLevel = frameData.m_numSections > 0 ? frameData.m_sections[frameData.m_numSections - 1].recLevel : 0;

	for (int i = frameData.m_numSections - 1; i >= 0; i--)
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
}

void CRenderPipelineProfiler::UpdateThreadTimings()
{
	const float weight = 8.0f / 9.0f;
	const uint32 fillThreadID = (uint32)gRenDev->GetMainThreadID();

	m_threadTimings.waitForMain = (gcpRendD3D->m_fTimeWaitForMain[fillThreadID] * (1.0f - weight) + m_threadTimings.waitForMain * weight);
	m_threadTimings.waitForRender = (gcpRendD3D->m_fTimeWaitForRender[fillThreadID] * (1.0f - weight) + m_threadTimings.waitForRender * weight);
	m_threadTimings.waitForGPU = (gcpRendD3D->m_fTimeWaitForGPU[fillThreadID] * (1.0f - weight) + m_threadTimings.waitForGPU * weight);
	m_threadTimings.gpuIdlePerc = (gcpRendD3D->m_fTimeGPUIdlePercent[fillThreadID] * (1.0f - weight) + m_threadTimings.gpuIdlePerc * weight);
	m_threadTimings.gpuFrameTime = (gcpRendD3D->m_fTimeProcessedGPU[fillThreadID] * (1.0f - weight) + m_threadTimings.gpuFrameTime * weight);
	m_threadTimings.frameTime = (iTimer->GetRealFrameTime() * (1.0f - weight) + m_threadTimings.frameTime * weight);
	m_threadTimings.renderTime = min((gcpRendD3D->m_fTimeProcessedRT[fillThreadID] * (1.0f - weight) + m_threadTimings.renderTime * weight), m_threadTimings.frameTime);
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

void CRenderPipelineProfiler::ComputeAverageStats(SFrameData& frameData)
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
		RPProfilerStats& basicStat = m_basicStats[processThreadID][i];
		basicStat.gpuTimeSmoothed = smoothWeightDataOld * m_basicStats[fillThreadID][i].gpuTimeSmoothed + smoothWeightDataNew * basicStat.gpuTime;
		float gpuTimeMax = std::max(basicStat._gpuTimeMaxNew, m_basicStats[fillThreadID][i]._gpuTimeMaxNew);
		basicStat._gpuTimeMaxNew = std::max(gpuTimeMax, basicStat.gpuTime);

		if (s_frameCounter % kUpdateFrequency == 0)
		{
			basicStat.gpuTimeMax = basicStat._gpuTimeMaxNew;
			basicStat._gpuTimeMaxNew = 0;
		}
	}

	for (uint32 i = 0; i < frameData.m_numSections; ++i)
	{
		SProfilerSection& section = frameData.m_sections[i];
		section.gpuTimeSmoothed = smoothWeightDataOld * section.gpuTimeSmoothed + smoothWeightDataNew * section.gpuTime;
		section.cpuTimeSmoothed = smoothWeightDataOld * section.cpuTimeSmoothed + smoothWeightDataNew * section.endTimeCPU.GetDifferenceInSeconds(section.startTimeCPU) * 1000.0f;
	}

	s_frameCounter += 1;
}

ILINE void CRenderPipelineProfiler::AddToStats(RPProfilerStats& outStats, SProfilerSection& section)
{
	outStats.gpuTime += section.gpuTime;
	outStats.cpuTime += section.endTimeCPU.GetDifferenceInSeconds(section.startTimeCPU) * 1000.0f;
	outStats.numDIPs += section.numDIPs;
	outStats.numPolys += section.numPolys;
}

ILINE void CRenderPipelineProfiler::SubtractFromStats(RPProfilerStats& outStats, SProfilerSection& section)
{
	outStats.gpuTime -= section.gpuTime;
	outStats.cpuTime -= section.endTimeCPU.GetDifferenceInSeconds(section.startTimeCPU) * 1000.0f;
	outStats.numDIPs -= section.numDIPs;
	outStats.numPolys -= section.numPolys;
}

void CRenderPipelineProfiler::UpdateBasicStats(uint32 frameDataIndex)
{
	RPProfilerStats* pBasicStats = m_basicStats[gRenDev->GetRenderThreadID()];

	ResetBasicStats(pBasicStats, false);

	bool bRecursivePass = false;
	SFrameData& frameData = m_frameData[frameDataIndex];

	for (uint32 i = 0; i < frameData.m_numSections; ++i)
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
	}

	AddToStats(pBasicStats[eRPPSTATS_OverallFrame], frameData.m_sections[0]);

	ComputeAverageStats(frameData);
}

void CRenderPipelineProfiler::DisplayDetailedPassStats(uint32 frameDataIndex)
{
#ifndef _RELEASE
	if (gEnv->pConsole->IsOpened())
		return;

	CD3D9Renderer* rd = gcpRendD3D;
	CRenderDisplayContext* pDC = rd->GetActiveDisplayContext();
	SFrameData& frameData = m_frameData[frameDataIndex];
	uint32 elemsPerColumn = (pDC->GetDisplayResolution()[1] - 60) / 16;

	// TODO: relative/normalized coordinate system in screen-space
	float sx = /*VIRTUAL_SCREEN_WIDTH */ float(pDC->GetDisplayResolution()[0]);
	float sy = /*VIRTUAL_SCREEN_HEIGHT*/ float(pDC->GetDisplayResolution()[1]);

	// Dim background to make text more readable
	IRenderAuxImage::Draw2dImage(0, 0, sx, sy, CRendererResources::s_ptexWhite->GetID(), 0, 0, 1, 1, 0, ColorF(0.05f, 0.05f, 0.05f, 0.5f));

	ColorF color = frameData.m_numSections >= SFrameData::kMaxNumSections ? Col_Red : ColorF(1.0f, 1.0f, 0.2f, 1);
	m_avgFrameTime = 0.8f * gEnv->pTimer->GetRealFrameTime() + 0.2f * m_avgFrameTime; // exponential moving average for frame time

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
		for (uint32 i = 0; i < frameData.m_numSections; ++i)
		{
			s_arrayTimes.push_back(frameData.m_sections[i].gpuTime);
		}
		std::sort(s_arrayTimes.begin(), s_arrayTimes.end());
		medianTimeGPU = s_arrayTimes[frameData.m_numSections / 2];
	}

	float frameTimeGPU = max(m_threadTimings.gpuFrameTime * 1000.0f, 0.0f);

	for (uint32 i = 0; i < frameData.m_numSections; ++i)
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
		if (!(section.flags & eProfileSectionFlags_MultithreadedSection))
		{
			IRenderAuxText::Draw2dLabel(xpos + 380, ypos, 1.5f, &color.r, false, "%.2fms", cpuTimeSmoothed);
		}
		else
		{
			IRenderAuxText::Draw2dLabel(xpos + 380, ypos, 1.5f, &color.r, false, " MT ");
		}
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

void CRenderPipelineProfiler::DisplayOverviewStats()
{
#ifndef _RELEASE
	if (gEnv->pConsole->IsOpened())
		return;

	CD3D9Renderer* rd = gcpRendD3D;

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

	//rd->SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);

	// Threading info
	{
		DebugUI::DrawTable(0.05f, 0.1f, 0.45f, 4, "Overview");

		float frameTime = m_threadTimings.frameTime;
		float mainThreadTime = max(m_threadTimings.frameTime - m_threadTimings.waitForRender, 0.0f);
		float renderThreadTime = max(m_threadTimings.renderTime - m_threadTimings.waitForGPU, 0.0f);
	#ifdef CRY_PLATFORM_ORBIS
		float gpuTime = max((100.0f - m_threadTimings.gpuIdlePerc) * frameTime * 0.01f, 0.0f);
	#else
		float gpuTime = max(m_threadTimings.gpuFrameTime, 0.0f);
	#endif
		float waitForGPU = max(m_threadTimings.waitForGPU, 0.0f);

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
