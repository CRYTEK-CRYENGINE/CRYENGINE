// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CrySystem/Profilers/IStatoscope.h>
#include "PipelineProfiler.h"
#include <CryNetwork/INetwork.h>
#include "StatoscopeRenderStats.h"
#include "DriverD3D.h"

#if ENABLE_STATOSCOPE

CGPUTimesDG::CGPUTimesDG(CD3D9Renderer* pRenderer)
	: m_pRenderer(pRenderer)
{
}

IStatoscopeDataGroup::SDescription CGPUTimesDG::GetDescription() const
{
	return IStatoscopeDataGroup::SDescription('i', "GPU Times",
		"['/GPUTimes/' (float Frame) (float OceanReflections) "
		"(float Scene/Overall) (float Scene/Decals) (float Scene/Forward) (float Scene/Water) "
		"(float Shadows/Overall) (float Shadows/Sun) (float Shadows/Per-Object) (float Shadows/Local) "
		"(float Lighting/Overall) (float Lighting/VoxelGI) "
		"(float VFX/Overall) (float VFX/Particles&Glass) (float VFX/Fog) (float VFX/Flares)]");
}

void CGPUTimesDG::Enable()
{
	IStatoscopeDataGroup::Enable();

	if (m_pRenderer->m_pPipelineProfiler)
	{
		m_pRenderer->m_pPipelineProfiler->SetEnabled(true);
	}
}

void CGPUTimesDG::Write(IStatoscopeFrameRecord& fr)
{
	const RPProfilerStats* pRPPStats = m_pRenderer->GetRPPStatsArray();
	if (pRPPStats)
	{
		fr.AddValue(pRPPStats[eRPPSTATS_OverallFrame].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_Recursion].gpuTime);

		fr.AddValue(pRPPStats[eRPPSTATS_SceneOverall].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_SceneDecals].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_SceneForward].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_SceneWater].gpuTime);

		fr.AddValue(pRPPStats[eRPPSTATS_ShadowsOverall].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_ShadowsSun].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_ShadowsSunCustom].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_ShadowsLocal].gpuTime);

		fr.AddValue(pRPPStats[eRPPSTATS_LightingOverall].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_LightingGI].gpuTime);

		fr.AddValue(pRPPStats[eRPPSTATS_VfxOverall].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_VfxTransparent].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_VfxFog].gpuTime);
		fr.AddValue(pRPPStats[eRPPSTATS_VfxFlares].gpuTime);
	}
}

CDetailedRenderTimesDG::CDetailedRenderTimesDG(CD3D9Renderer* pRenderer)
	: m_pRenderer(pRenderer)
{
}

IStatoscopeDataGroup::SDescription CDetailedRenderTimesDG::GetDescription() const
{
	return IStatoscopeDataGroup::SDescription('I', "Detailed Rendering Timers",
		"['/RenderTimes/$' (float GPUTime) (float CPUTime) (int DIPs) (int Polygons)]");
}

void CDetailedRenderTimesDG::Enable()
{
	IStatoscopeDataGroup::Enable();

	if (m_pRenderer->m_pPipelineProfiler)
	{
		m_pRenderer->m_pPipelineProfiler->SetEnabled(true);
	}
}

void CDetailedRenderTimesDG::Write(IStatoscopeFrameRecord& fr)
{
	string path = "";
	int curRecLevel = 1;
	char buf[1024];
	std::stack<int> recLevelStack;
	recLevelStack.push(0);
	int disambigCounter = 0;
	for (auto it = m_stats->begin(); it != m_stats->end(); ++it)
	{
		const RPProfilerDetailedStats& st = *it;
		if (st.recLevel < 0)
			continue;

		if (st.recLevel > 0)
		{
			while (curRecLevel > st.recLevel)
			{
				recLevelStack.pop();
				curRecLevel--;
			}
			if (curRecLevel < st.recLevel)
			{
				recLevelStack.push(strlen(buf));
				curRecLevel++;
			}
		}

		// Ensure nodes at the same place in the tree have unique names
		// Assumes all identically-named nodes at the same level will appear consecutively
		if (it != m_stats->begin() && (it - 1)->recLevel == st.recLevel && strncmp((it - 1)->name, st.name, strlen(st.name)) == 0)
		{
			cry_sprintf(&buf[recLevelStack.top()], 1024 - recLevelStack.top(), "%s%d/", st.name, ++disambigCounter);
		}
		else
		{
			disambigCounter = 0;
			cry_sprintf(&buf[recLevelStack.top()], 1024-recLevelStack.top(), "%s/", st.name);
		}

		fr.AddValue(buf);
		fr.AddValue(st.gpuTime);
		fr.AddValue(st.cpuTime);
		fr.AddValue(st.numDIPs);
		fr.AddValue(st.numPolys);
	}
}

uint32 CDetailedRenderTimesDG::PrepareToWrite()
{
	m_stats = m_pRenderer->GetRPPDetailedStatsArray();
	return m_stats->size();
}

CGraphicsDG::CGraphicsDG(CD3D9Renderer* pRenderer)
{
	m_pRenderer = pRenderer;
	m_gpuUsageHistory.reserve(GPU_HISTORY_LENGTH);
	m_frameTimeHistory.reserve(GPU_HISTORY_LENGTH);

	m_lastFrameScreenShotRequested = 0;
	m_totFrameTime = 0.f;

	ResetGPUUsageHistory();

	REGISTER_CVAR2("e_StatoscopeScreenCapWhenGPULimited", &m_cvarScreenCapWhenGPULimited, 0, VF_NULL, "Statoscope will take a screen capture when we are GPU limited");
}

void CGraphicsDG::ResetGPUUsageHistory()
{
	m_gpuUsageHistory.clear();
	m_frameTimeHistory.clear();

	for (uint32 i = 0; i < GPU_HISTORY_LENGTH; i++)
	{
		m_gpuUsageHistory.push_back(0.f);
		m_frameTimeHistory.push_back(0.f);
	}
	m_nFramesGPULmited = 0;
	m_totFrameTime = 0;
}

void CGraphicsDG::TrackGPUUsage(float gpuLoad, float frameTimeMs, int totalDPs)
{
	float oldGPULoad = m_gpuUsageHistory[0];

	if (oldGPULoad >= 99.f)
	{
		m_nFramesGPULmited--;
	}
	if (gpuLoad >= 99.f)
	{
		m_nFramesGPULmited++;
	}

	m_gpuUsageHistory.erase(m_gpuUsageHistory.begin());
	m_gpuUsageHistory.push_back(gpuLoad);

	float oldFrameTime = m_frameTimeHistory[0];

	m_totFrameTime -= oldFrameTime;
	m_totFrameTime += frameTimeMs;

	m_frameTimeHistory.erase(m_frameTimeHistory.begin());
	m_frameTimeHistory.push_back(frameTimeMs);

	int currentFrame = gEnv->pRenderer->GetFrameID(false);

	//Don't spam screen shots
	bool bScreenShotAvaliable = (currentFrame >= (m_lastFrameScreenShotRequested + SCREEN_SHOT_FREQ));

	bool bFrameTimeToHigh = ((m_totFrameTime / (float)GPU_HISTORY_LENGTH) > (float)GPU_TIME_THRESHOLD_MS);
	bool bGpuLimited = (m_nFramesGPULmited == GPU_HISTORY_LENGTH);

	bool bDPLimited = (totalDPs > DP_THRESHOLD);

	if (bScreenShotAvaliable && (bDPLimited || (bGpuLimited && bFrameTimeToHigh)))
	{
		if (bDPLimited)
		{
			string userMarkerName;
			userMarkerName.Format("TotalDPs: %d, Screen Shot taken", totalDPs);
			gEnv->pStatoscope->AddUserMarker("DPLimited", userMarkerName);
			//printf("[Statoscope] DP Limited: %d, requesting ScreenShot frame: %d\n", totalDPs, currentFrame);
		}
		else
		{
			gEnv->pStatoscope->AddUserMarker("GPULimited", "Screen Shot taken");
			//printf("[Statoscope] GPU Limited, requesting ScreenShot frame: %d\n", currentFrame);
		}

		gEnv->pStatoscope->RequestScreenShot();
		m_lastFrameScreenShotRequested = currentFrame;
	}
}

IStatoscopeDataGroup::SDescription CGraphicsDG::GetDescription() const
{
	return IStatoscopeDataGroup::SDescription('g', "graphics",
	                                          "['/Graphics/' (float GPUUsageInPercent) (float GPUFrameLengthInMS) (int numTris) (int numDrawCalls) "
	                                          "(int numShadowDrawCalls) (int numGeneralDrawCalls) (int numTransparentDrawCalls) (int numTotalDrawCalls) "
	                                          "(int numPostEffects) (int numForwardLights) (int numForwardShadowCastingLights)]");
}

void CGraphicsDG::Write(IStatoscopeFrameRecord& fr)
{
	IRenderer::SRenderTimes renderTimes;
	m_pRenderer->GetRenderTimes(renderTimes);

	float GPUUsageInPercent = 100.f - renderTimes.fTimeGPUIdlePercent;
	fr.AddValue(GPUUsageInPercent);
	fr.AddValue(GPUUsageInPercent * 0.01f * renderTimes.fTimeProcessedGPU * 1000.0f);

	int numTris, numShadowVolPolys;
	m_pRenderer->GetPolyCount(numTris, numShadowVolPolys);
	fr.AddValue(numTris);

	int numDrawCalls, numShadowDrawCalls, numGeneralDrawCalls, numTransparentDrawCalls;
	numDrawCalls = m_pRenderer->GetCurrentNumberOfDrawCalls();
	numShadowDrawCalls = m_pRenderer->GetCurrentNumberOfDrawCalls(1 << EFSLIST_SHADOW_GEN);
	numGeneralDrawCalls = m_pRenderer->GetCurrentNumberOfDrawCalls(1 << EFSLIST_GENERAL);
	numTransparentDrawCalls = m_pRenderer->GetCurrentNumberOfDrawCalls(1 << EFSLIST_TRANSP_AW) + m_pRenderer->GetCurrentNumberOfDrawCalls(1 << EFSLIST_TRANSP_BW);
	fr.AddValue(numDrawCalls);
	fr.AddValue(numShadowDrawCalls);
	fr.AddValue(numGeneralDrawCalls);
	fr.AddValue(numTransparentDrawCalls);
	fr.AddValue(numDrawCalls + numShadowDrawCalls);

	int nNumActivePostEffects = 0;
	m_pRenderer->EF_Query(EFQ_NumActivePostEffects, nNumActivePostEffects);
	fr.AddValue(nNumActivePostEffects);

	PodArray<SRenderLight*>* pLights = gEnv->p3DEngine->GetDynamicLightSources();
	int nDynamicLights = (int)pLights->Count();
	int nShadowCastingLights = 0;

	// for some stupid reason nDynamicLights can be huge (like 806595888)
	if (nDynamicLights > 100)
	{
		nDynamicLights = 0;
	}

	for (int i = 0; i < nDynamicLights; i++)
	{
		SRenderLight* pLight = pLights->GetAt(i);

		if (pLight->m_Flags & DLF_CASTSHADOW_MAPS)
		{
			nShadowCastingLights++;
		}
	}

	fr.AddValue(nDynamicLights);
	fr.AddValue(nShadowCastingLights);

	if (m_cvarScreenCapWhenGPULimited)
	{
		TrackGPUUsage(GPUUsageInPercent, gEnv->pTimer->GetRealFrameTime() * 1000.0f, numDrawCalls + numShadowDrawCalls);
	}
}

void CGraphicsDG::Enable()
{
	IStatoscopeDataGroup::Enable();
}

CPerformanceOverviewDG::CPerformanceOverviewDG(CD3D9Renderer* pRenderer)
	: m_pRenderer(pRenderer)
{
}

IStatoscopeDataGroup::SDescription CPerformanceOverviewDG::GetDescription() const
{
	return IStatoscopeDataGroup::SDescription('O', "frame performance overview",
	                                          "['/Overview/' (float frameLengthInMS) (float lostProfilerTimeInMS) (float profilerAdjFrameLengthInMS)"
	                                          "(float MTLoadInMS) (float RTLoadInMS) (float GPULoadInMS) (float NetTLoadInMS) "
	                                          "(int numDrawCalls) (float FrameScaleFactor)]");
}

void CPerformanceOverviewDG::Write(IStatoscopeFrameRecord& fr)
{
	IFrameProfileSystem* pFrameProfileSystem = gEnv->pSystem->GetIProfileSystem();
	const float frameLengthSec = gEnv->pTimer->GetRealFrameTime();
	const float frameLengthMs = frameLengthSec * 1000.0f;

	IRenderer::SRenderTimes renderTimes;
	gEnv->pRenderer->GetRenderTimes(renderTimes);

	SNetworkPerformance netPerformance;
	if (gEnv->pNetwork)
		gEnv->pNetwork->GetPerformanceStatistics(&netPerformance);
	else
		netPerformance.m_threadTime = 0.0f;

	int numDrawCalls, numShadowDrawCalls;
	gEnv->pRenderer->GetCurrentNumberOfDrawCalls(numDrawCalls, numShadowDrawCalls);

	fr.AddValue(frameLengthMs);
	fr.AddValue(pFrameProfileSystem ? pFrameProfileSystem->GetLostFrameTimeMS() : -1.f);
	fr.AddValue(frameLengthMs - (pFrameProfileSystem ? pFrameProfileSystem->GetLostFrameTimeMS() : 0.f));
	fr.AddValue((frameLengthSec - renderTimes.fWaitForRender) * 1000.0f);
	fr.AddValue((renderTimes.fTimeProcessedRT - renderTimes.fWaitForGPU) * 1000.f);
	fr.AddValue(gEnv->pRenderer->GetGPUFrameTime() * 1000.0f);
	fr.AddValue(netPerformance.m_threadTime * 1000.0f);
	fr.AddValue(numDrawCalls + numShadowDrawCalls);

	Vec2 scale = Vec2(0.0f, 0.0f);
	gEnv->pRenderer->EF_Query(EFQ_GetViewportDownscaleFactor, scale);
	fr.AddValue(scale.x);
}

void CPerformanceOverviewDG::Enable()
{
	IStatoscopeDataGroup::Enable();
}

#endif // ENABLE_STATOSCOPE
