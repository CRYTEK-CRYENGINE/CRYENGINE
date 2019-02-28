// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   frameprofilerender.cpp
//  Version:     v1.00
//  Created:     24/6/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: Rendering of FrameProfiling information.
// -------------------------------------------------------------------------
//  History: Some ideas taken from Jonathan Blow`s profiler from GDmag article.
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "FrameProfileSystem.h"
#include <CryRenderer/IRenderer.h>
#include <CryInput/IInput.h>
#include <CrySystem/ILog.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CrySystem/ITextModeConsole.h>

#include <CrySystem/File/CryFile.h>

#include "Statistics.h"

#include <CryCore/Platform/CryWindows.h>

#if CRY_PLATFORM_WINDOWS
	#include <psapi.h> // requires <windows.h>
LINK_SYSTEM_LIBRARY("psapi.lib")
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	#include <dlfcn.h>
#endif

#ifdef USE_FRAME_PROFILER

	#define VARIANCE_MULTIPLIER       2.0f

//! 5 seconds from hot to cold in peaks.
	#define MAX_DISPLAY_ROWS 80

extern int CryMemoryGetAllocatedSize();
extern int CryMemoryGetPoolSize();

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)

enum EProfMode
{
	ePM_CPU_MODE      = 0,
	ePM_THREAD_MODE   = 1,
	ePM_BLOCKING_MODE = 2,

};

		#define COL_DIFF_JOBS            80
		#define ROW_DIFF_THREAD          0
		#define ROW_DIFF_THREAD_HEADER   (ROW_DIFF_THREAD * 2) // Different ROW_SIZE used when rendering headers
		#define ROW_DIFF_BLOCKING        20
		#define ROW_DIFF_BLOCKING_HEADER (ROW_DIFF_BLOCKING * 2) // Different ROW_SIZE used when rendering headers

	#endif // JOBMANAGER_SUPPORT_FRAMEPROFILER

	#define COL_DIFF_HISTOGRAMS 50

namespace FrameProfileRenderConstants
{
const float c_yScale = 1.3f;
const int c_yStepSizeText = (int)(10.f * c_yScale);
const int c_yStepSizeTextMeter = (int)(8.f * c_yScale);
const float c_yNextControlGap = 12.f * c_yScale;
const float c_fontScale = 1.0f * c_yScale;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::DrawLabel(float col, float row, float* fColor, float glow, const char* szText, float fScale)
{
	float ColumnSize = COL_SIZE;
	float RowSize = ROW_SIZE;

	if (m_pRenderer)
	{
		ColorF color;
		float scale = fScale * 1.3f;
		int flags = eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace;
		color[0] = fColor[0];
		color[1] = fColor[1];
		color[2] = fColor[2];

		if (glow > 0.1f)
		{
			color[3] = glow;
			IRenderAuxText::DrawText(Vec3(ColumnSize * col + 1, m_baseY + RowSize * row + 1 - m_offset, 0.5f), scale, color, flags, szText);
		}

		color[3] = fColor[3];
		IRenderAuxText::DrawText(Vec3(ColumnSize * col + 1, m_baseY + RowSize * row + 1 - m_offset, 0.5f), scale, color, flags, szText);
	}

	if (ITextModeConsole* pTC = gEnv->pSystem->GetITextModeConsole())
	{
		pTC->PutText((int)col, (int)(m_textModeBaseExtra + row + std::max(0.0f, (m_baseY - 120.0f) / ROW_SIZE)), szText);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::DrawRect(float x1, float y1, float x2, float y2, float* fColor)
{
	int w = m_pRenderer->GetOverlayWidth();
	int h = m_pRenderer->GetOverlayHeight();

	y1 -= m_offset;
	y2 -= m_offset;
	//float dx = 800.0f/w;
	//float dy = 600.0f/h;
	float dx = 1.0f / w;
	float dy = 1.0f / h;
	x1 *= dx;
	x2 *= dx;
	y1 *= dy;
	y2 *= dy;

	ColorB col((uint8)(fColor[0] * 255.0f), (uint8)(fColor[1] * 255.0f), (uint8)(fColor[2] * 255.0f), (uint8)(fColor[3] * 255.0f));

	IRenderAuxGeom* pAux = m_pRenderer->GetIRenderAuxGeom();
	SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
	flags.SetMode2D3DFlag(e_Mode2D);
	pAux->SetRenderFlags(flags);
	pAux->DrawLine(Vec3(x1, y1, 0), col, Vec3(x2, y1, 0), col);
	pAux->DrawLine(Vec3(x1, y2, 0), col, Vec3(x2, y2, 0), col);
	pAux->DrawLine(Vec3(x1, y1, 0), col, Vec3(x1, y2, 0), col);
	pAux->DrawLine(Vec3(x2, y1, 0), col, Vec3(x2, y2, 0), col);
}

//////////////////////////////////////////////////////////////////////////
inline float CalculateVarianceFactor(float value, float variance)
{
	//variance = fabs(variance - value*value);
	float difference = (float)sqrt_tpl(variance);

	const float VALUE_EPSILON = 0.000001f;
	value = (float)fabs(value);
	// Prevent division by zero.
	if (value < VALUE_EPSILON)
	{
		return 0;
	}
	float factor = 0;
	if (value > 0.01f)
		factor = (difference / value) * VARIANCE_MULTIPLIER;

	return factor;
}

//////////////////////////////////////////////////////////////////////////
inline void CalculateColor(float value, float variance, float* outColor, float& glow)
{
	float ColdColor[4] = { 0.15f, 0.9f, 0.15f, 1 };
	float HotColor[4] = { 1, 1, 1, 1 };

	glow = 0;

	float factor = CalculateVarianceFactor(value, variance);
	if (factor < 0)
		factor = 0;
	if (factor > 1)
		factor = 1;

	// Interpolate Hot to Cold color with variance factor.
	for (int k = 0; k < 4; k++)
		outColor[k] = HotColor[k] * factor + ColdColor[k] * (1.0f - factor);

	// Figure out whether to start up the glow as well.
	const float GLOW_RANGE = 0.5f;
	const float GLOW_ALPHA_MAX = 0.5f;
	float glow_alpha = (factor - GLOW_RANGE) / (1 - GLOW_RANGE);
	glow = glow_alpha * GLOW_ALPHA_MAX;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::AddDisplayedProfiler(CFrameProfiler* pProfiler, int level)
{
	bool bExpended = pProfiler->m_bExpended;

	int newLevel = level + 1;
	if (!IsSubSystemFiltered(pProfiler))
	{
		SProfilerDisplayInfo info;
		info.level = level;
		info.pProfiler = pProfiler;
		m_displayedProfilers.push_back(info);
	}
	else
	{
		bExpended = true;
		newLevel = level;
	}
	// Find childs.
	//@TODO Very Slow, optimize that.
	if (bExpended && pProfiler->m_bHaveChildren)
	{
		for (int i = 0; i < (int)m_pProfilers->size(); i++)
		{
			CFrameProfiler* pCur = (*m_pProfilers)[i];
			if (pCur->m_pParent == pProfiler)
				AddDisplayedProfiler(pCur, newLevel);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::CalcDisplayedProfilers()
{
	if (m_bDisplayedProfilersValid)
		return;

	m_bDisplayedProfilersValid = true;
	m_displayedProfilers.reserve(m_pProfilers->size());
	m_displayedProfilers.resize(0);

	//////////////////////////////////////////////////////////////////////////
	// Get all displayed profilers.
	//////////////////////////////////////////////////////////////////////////
	if (m_displayQuantity == TOTAL_TIME || m_bEnableHistograms)
	{
		if (m_bEnableHistograms)
		{
			// In extended mode always add first item selected profiler.
			if (m_pGraphProfiler)
			{
				SProfilerDisplayInfo info;
				info.level = 0;
				info.pProfiler = m_pGraphProfiler;
				m_displayedProfilers.push_back(info);
			}
		}
		// Go through all profilers.
		for (int i = 0; i < (int)m_pProfilers->size(); i++)
		{
			CFrameProfiler* pProfiler = (*m_pProfilers)[i];
			if (!pProfiler->m_pParent && pProfiler->m_displayedValue >= 0.01f)
			{
				//pProfiler->m_bExpended = true;
				AddDisplayedProfiler(pProfiler, 0);
			}
		}
		if (m_displayedProfilers.empty())
			m_bDisplayedProfilersValid = false;
		return;
	}

	// Go through all profilers.
	for (int i = 0; i < (int)m_pProfilers->size(); i++)
	{
		CFrameProfiler* pProfiler = (*m_pProfilers)[i];
		// Skip this profiler if its filtered out.
		if (IsSubSystemFiltered(pProfiler))
			continue;

		SProfilerDisplayInfo info;
		info.averageCount = pProfiler->m_count.Average();
		if (!info.averageCount)
			continue;
		if (m_displayQuantity != COUNT_INFO)
		{
			if (pProfiler->m_displayedValue < profile_min_display_ms)
				continue;
		}
		info.level = 0;
		info.pProfiler = pProfiler;
		m_displayedProfilers.push_back(info);
	}

	if (m_displayQuantity == COUNT_INFO)
		stl::sort(m_displayedProfilers, [](SProfilerDisplayInfo const& info) { return -info.averageCount; });
	else
		stl::sort(m_displayedProfilers, [](SProfilerDisplayInfo const& info) { return -info.pProfiler->m_displayedValue; });
	if ((int)m_displayedProfilers.size() > m_maxProfileCount)
		m_displayedProfilers.resize(m_maxProfileCount);
}

//////////////////////////////////////////////////////////////////////////
CFrameProfiler* CFrameProfileSystem::GetSelectedProfiler()
{
	if (m_displayedProfilers.empty())
		return 0;
	if (m_selectedRow < 0)
		m_selectedRow = 0;
	if (m_selectedRow > (int)m_displayedProfilers.size() - 1)
		m_selectedRow = (int)m_displayedProfilers.size() - 1;
	return m_displayedProfilers[m_selectedRow].pProfiler;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::Render()
{
	// EndFrame();

	m_textModeBaseExtra = 0;

	static int memProfileValueOld = 0;
	if (memProfileValueOld != profile_meminfo)
	{
		m_bLogMemoryInfo = true;
		memProfileValueOld = profile_meminfo;
	}
	if (profile_meminfo)
		RenderMemoryInfo();

	if (!m_bDisplay)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	m_textModeBaseExtra = 2;
	ROW_SIZE = 10;
	COL_SIZE = 11;

	m_baseY = profile_row * ROW_SIZE;

	m_pRenderer = GetISystem()->GetIRenderer();

	if (!m_bCollectionPaused)
	{
		m_selectedRow = -1;
	}

	float colText = profile_col - 4 * gEnv->IsDedicated();
	float colExtended = 1.0f;
	float row = 0;

	if (m_pRenderer)
	{
		m_pRenderer->GetIRenderAuxGeom()->SetOrthographicProjection(true, 0.0f, static_cast<float>(m_pRenderer->GetOverlayWidth()), static_cast<float>(m_pRenderer->GetOverlayHeight()), 0.0f);
	}

	//////////////////////////////////////////////////////////////////////////
	// Check if displayed profilers must be recalculated.
	if (m_displayQuantity == TOTAL_TIME || m_bEnableHistograms)
	{
		if (m_bCollectionPaused)
			m_bDisplayedProfilersValid = false;
	}
	else
		m_bDisplayedProfilersValid = false;
	//////////////////////////////////////////////////////////////////////////

	// Calculate which profilers must be displayed, and sort by importance.
	if (m_displayQuantity != SUBSYSTEM_INFO)
		CalcDisplayedProfilers();

	if (m_bEnableHistograms)
	{
		m_baseY = 50;
	}

	if (m_bCollectionPaused)
	{
		float PausedColor[4] = { 1, 0.3f, 0, 1 };

		char szText[32] = "";
		cry_strcat(szText, "Paused");

	#if CRY_PLATFORM_WINDOWS
		if (GetKeyState(VK_SCROLL) & 1)
		{
			cry_strcat(szText, " - [Scroll-Lock Active]");
		}
	#endif

		DrawLabel(colText + 10, row - 5, PausedColor, 0.2f, szText, 1.2f);
	}

	if (m_displayQuantity != PEAK_TIME)
	{
		if (m_bEnableHistograms)
			RenderProfilerHeader(colExtended, row, m_bEnableHistograms);
		else
			RenderProfilerHeader(colText, row, m_bEnableHistograms);

		if (m_bEnableHistograms)
		{
			ROW_SIZE = (float)m_histogramsHeight + 4;
		}

		//////////////////////////////////////////////////////////////////////////
		if (m_bEnableHistograms)
			RenderProfilers(colExtended, 0, true);
		else if (m_displayQuantity == SUBSYSTEM_INFO)
			RenderSubSystems(colText, 0);
		else if (m_displayQuantity != PEAKS_ONLY)
			RenderProfilers(colText, 0, false);
		//////////////////////////////////////////////////////////////////////////
	}

	// Render Peaks.
	if (m_displayQuantity == PEAK_TIME || profile_graph || profile_pagefaults)
	{
		DrawGraph();
	}

	float fpeaksLastRow = 0;

	if (m_peaks.size() > 0 && m_displayQuantity != PEAK_TIME && profile_peak_display > 0.0f)
	{
		fpeaksLastRow = RenderPeaks();
	}

	if (profile_additionalsub)
	{
		float colPeaks = 16.0f;
		RenderSubSystems(colPeaks, 30.0f);  // can visually collide with waiting peaks
	}

	if (m_pRenderer)
	{
		m_pRenderer->GetIRenderAuxGeom()->SetOrthographicProjection(false);
	}

	if (m_bEnableHistograms)
		RenderHistograms();

	// StartFrame();
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderProfilers(float col, float row, bool bExtended)
{
	//  float HeaderColor[4] = { 1,1,0,1 };
	float CounterColor[4] = { 0, 0.8f, 1, 1 };

	// Header.
	m_baseY += 40;
	row = 0;

	CFrameProfiler* pSelProfiler = 0;
	if (m_bCollectionPaused)
		pSelProfiler = GetSelectedProfiler();

	if (CFrameProfileSystem::profile_log)
	{
		CryLogAlways("====================Start Profiler Frame %d, Time %.2f ======================", gEnv->pRenderer->GetFrameID(false), m_frameSecAvg * 1000.f);
		CryLogAlways("|\tCount\t|\tSelf\t|\tTotal\t|\tModule\t|");
		CryLogAlways("|\t____\t|\t_____\t|\t_____\t|\t_____\t|");

		int logType = abs(CFrameProfileSystem::profile_log);

		for (int i = 0; i < (int)m_displayedProfilers.size(); i++)
		{
			CFrameProfiler* pProfiler = m_displayedProfilers[i].pProfiler;

			if (logType == 1)
			{
				uint cave = pProfiler->m_count.Average();
				float fTotalTimeMs = pProfiler->m_totalTime.Average();
				float fSelfTimeMs = pProfiler->m_selfTime.Average();
				CryLogAlways("|\t%d\t|\t%.2f\t|\t%.2f%%\t|\t%s\t|\t %s", cave, fSelfTimeMs, fTotalTimeMs, GetModuleName(pProfiler), GetFullName(pProfiler));
			}
			else if (logType == 2)
			{
				int c_min = pProfiler->m_count.Min();
				int c_max = pProfiler->m_count.Max();
				float t1_min = pProfiler->m_totalTime.Min();
				float t1_max = pProfiler->m_totalTime.Max();
				float t0_min = pProfiler->m_selfTime.Min();
				float t0_max = pProfiler->m_selfTime.Max();
				CryLogAlways("|\t%d/%d\t|\t%.2f/%.2f\t|\t%.2f/%.2f%%\t|\t%s\t|\t %s", c_min, c_max, t0_min, t0_max, t1_min, t1_max, GetModuleName(pProfiler), GetFullName(pProfiler));
			}
		}

		CryLogAlways("======================= End Profiler Frame %d ==========================", gEnv->pRenderer->GetFrameID(false));
		if (CFrameProfileSystem::profile_log > 0) // reset logging so only logs one frame.
			CFrameProfileSystem::profile_log = 0;
	}

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	if (m_displayQuantity != STALL_TIME)
	{
		const float cConvFactor = 1.f / 1000.f;
		char szText[128];
		float colTextOfs = 3.0f + 4 * gEnv->IsDedicated();

		float colCountOfs = -4.5f - 3 * gEnv->IsDedicated();
		float glow = 0;
		float rowOrig = row;
		float ValueColor[4] = { 0, 1, 0, 1 };

		// DISPLAY JOBS

		JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer* pActiveFrameStats[] =
		{
			GetActiveFrameStats(JobManager::eBET_Thread),     // Display none-blocking jobs
			GetActiveFrameStats(JobManager::eBET_Blocking),   // Display blocking jobs
		};

		float rowStartPos[] =
		{
			(float)ROW_DIFF_THREAD,
			(float)ROW_DIFF_BLOCKING,
		};

		for (uint32 i = 0; i < CRY_ARRAY_COUNT(pActiveFrameStats); i++)
		{
			if (!pActiveFrameStats[i])
				continue;

			if (i == 0)
				ROW_SIZE = 20.0f;
			else
				ROW_SIZE = 10.0f;

			row = rowStartPos[i];
			JobManager::IWorkerBackEndProfiler::TJobFrameStatsContainer& rActiveFrameStats = *pActiveFrameStats[i];

			for (uint32 j = 0, nSize = rActiveFrameStats.size(); j < nSize; ++j)
			{
				const JobManager::SJobFrameStats& crProfData = rActiveFrameStats[j];
				float value = (float)crProfData.usec * cConvFactor;

				cry_sprintf(szText, "%4.2f", value);
				DrawLabel(col + COL_DIFF_JOBS, row, ValueColor, 0, szText);

				if (crProfData.count > 1)
				{
					cry_sprintf(szText, "%6u/", crProfData.count);
					DrawLabel(col + colCountOfs + COL_DIFF_JOBS, row, CounterColor, 0, szText);
				}

				DrawLabel(col + colTextOfs + COL_DIFF_JOBS + 0.5f, row, ValueColor, glow, crProfData.cpName);
				row += 0.5f;
			}
			row = rowOrig;
		}
	}
	#endif // JOBMANAGER_SUPPORT_FRAMEPROFILER

	int width  = GetISystem()->GetViewCamera().GetViewSurfaceX();
	int height = GetISystem()->GetViewCamera().GetViewSurfaceZ();

	// Go through all profilers.
	for (int i = 0; i < (int)m_displayedProfilers.size(); i++)
	{
		SProfilerDisplayInfo& dispInfo = m_displayedProfilers[i];
		CFrameProfiler* pProfiler = m_displayedProfilers[i].pProfiler;
		//filter stall profilers (caused by averaging)
		if (pProfiler && m_displayQuantity == STALL_TIME)
			continue;
		if (i > m_selectedRow + MAX_DISPLAY_ROWS)
		{
			break;
		}

		float rectX1 = col * COL_SIZE;
		float rectX2 = width - 2.0f;
		float rectY1 = m_baseY + row * ROW_SIZE + 2;
		float rectY2 = m_baseY + (row + 1) * ROW_SIZE + 2;

		dispInfo.x = rectX1;
		dispInfo.y = rectY1;

		if (dispInfo.y - m_offset + ROW_SIZE >= height)
			continue;

		if (i == m_selectedRow && m_bCollectionPaused)
		{
			float SelColor[4] = { 1, 1, 1, 1 };
			DrawRect(rectX1, rectY1, rectX2, rectY2, SelColor);
		}

		RenderProfiler(pProfiler, dispInfo.level, col, row, bExtended, (pSelProfiler == pProfiler));
		row += 1.0f;
	}
	m_baseY -= 40;
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderProfilerHeader(float col, float row, bool bExtended)
{
	char szText[256];
	char szTitle[32];
	float MainHeaderColor[4] = { 0, 1, 1, 1 };
	float HeaderColor[4] = { 1, 1, 0, 1 };
	float CounterColor[4] = { 0, 0.8f, 1, 1 };
	float PausedColor[4] = { 1, 0.3f, 0, 1 };
	const float origCol = col;

	bool bShowFrameTimeSummary = false;

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	float rowOrig = row;
	float colOrig = col;

	for (int mode = 0; mode < ((m_displayQuantity == STALL_TIME) ? 1 : 3); ++mode)
	{
		switch (mode)
		{
		case ePM_CPU_MODE:
			cry_strcpy(szTitle, "CPU");
			bShowFrameTimeSummary = true;
			break;
		case ePM_THREAD_MODE:
			cry_strcpy(szTitle, "JOBS (NONE-BLOCKING)");
			bShowFrameTimeSummary = false;
			col = colOrig + COL_DIFF_JOBS;
			row = rowOrig + ROW_DIFF_THREAD_HEADER;
			break;
		case ePM_BLOCKING_MODE:
			cry_strcpy(szTitle, "JOBS (BLOCKING)");
			bShowFrameTimeSummary = false;
			col = colOrig + COL_DIFF_JOBS;
			row = rowOrig + ROW_DIFF_BLOCKING_HEADER;
			break;
		}
		;

	#endif
	const char* sValueName = "Time";
	if (m_bMemoryProfiling)
	{
		sValueName = "KB(s)";
	}

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	if (mode != ePM_CPU_MODE)
		cry_strcpy(szText, "Profile Mode: Self Time");
	else
	{
	#endif

	cry_strcpy(szText, "");
	// Draw general statistics.
	switch ((int)m_displayQuantity)
	{
	case SELF_TIME:
	case SELF_TIME_EXTENDED:
		cry_strcpy(szText, "Profile Mode: Self Time");
		break;
	case TOTAL_TIME:
	case TOTAL_TIME_EXTENDED:
		cry_strcpy(szText, "Profile Mode: Hierarchical Time");
		break;
	case PEAK_TIME:
		cry_strcpy(szText, "Profile Mode: Peak Time");
		break;
	case COUNT_INFO:
		cry_strcpy(szText, "Profile Mode: Calls Number");
		break;
	case SUBSYSTEM_INFO:
		cry_strcpy(szText, "Profile Mode: Subsystems");
		break;
	case STANDARD_DEVIATION:
		cry_strcpy(szText, "Profile Mode: Standard Deviation");
		sValueName = "StdDev";
		break;
	case ALLOCATED_MEMORY:
		cry_strcpy(szText, "Profile Mode: Memory Allocations");
		sValueName = "KB(s)";
		break;
	case ALLOCATED_MEMORY_BYTES:
		cry_strcpy(szText, "Profile Mode: Memory Allocations (Bytes)");
		sValueName = "Bytes";
		break;
	case PEAKS_ONLY:
		cry_strcpy(szText, "Profile Mode: Peaks Only");
		sValueName = "Bytes";
		break;
	}

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
}
	#endif
	if (m_bCollectionPaused)
		cry_strcat(szText, " (Paused)");

	if (!m_filterThreadName.empty())
	{
		cry_strcat(szText, " (Thread:");
		cry_strcat(szText, m_filterThreadName.c_str());
		cry_strcat(szText, ")");
	}

	row--;
	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	if (m_displayQuantity == STALL_TIME)
	{
		float cStallHeaderCol[4] = { 1, 0.1f, 0.1f, 1 };
		DrawLabel(col, row - 1, cStallHeaderCol, 0, "CPU stalls (waiting for GPU)");
	}
	else
	{
		DrawLabel(col, row - 1, MainHeaderColor, 0, szTitle);
	}
	#endif

	if (!m_bCollectionPaused)
	{
		DrawLabel(col, row++, MainHeaderColor, 0, szText);
	}
	else
	{
		DrawLabel(col, row++, PausedColor, 0.2f, szText, 1.1f);
	}

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	if (bShowFrameTimeSummary)
	{
	#endif
	if (m_displayQuantity != STALL_TIME && m_displayQuantity != PEAKS_ONLY)
	{
		cry_sprintf(szText, "FrameTime: %4.2fms, OverheadTime: %4.2fms, LostTime: %4.2fms", m_frameSecAvg * 1000.f, m_frameOverheadSecAvg * 1000.f, m_frameLostSecAvg * 1000.f);
		if (m_nPagesFaultsPerSec)
		{
			const size_t len = strlen(szText);
			cry_sprintf(&szText[len], sizeof(szText) - len, ", PF/Sec: %d", m_nPagesFaultsPerSec);
		}
		DrawLabel(col, row++, MainHeaderColor, 0, szText);
	}
	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
}
else
	row++;
	#endif
	// Header.
	if (bExtended)
	{
		row = 0;
		m_baseY += 24;
		DrawLabel(col, row, HeaderColor, 0, "Max");
		DrawLabel(col + 5, row, HeaderColor, 0, "Min");
		DrawLabel(col + 10, row, HeaderColor, 0, "Ave");
		if (m_displayQuantity == TOTAL_TIME_EXTENDED)
			DrawLabel(col + 15, row, HeaderColor, 0, "Self");
		else
			DrawLabel(col + 15, row, HeaderColor, 0, "Now");
		DrawLabel(col + 2, row, CounterColor, 0, "/cnt");
		DrawLabel(col + 5 + 2, row, CounterColor, 0, "/cnt");
		DrawLabel(col + 10 + 2, row, CounterColor, 0, "/cnt");
		DrawLabel(col + 15 + 2, row, CounterColor, 0, "/cnt");
		m_baseY -= 24;
	}
	else if (m_displayQuantity == SUBSYSTEM_INFO)
	{
		DrawLabel(col, row, HeaderColor, 0, sValueName);
		DrawLabel(col + 9, row, HeaderColor, 0, "Waiting");
		DrawLabel(col + 18, row, HeaderColor, 0, "Subsystem");
	}
	else if (m_displayQuantity != PEAKS_ONLY)
	{
		//		col = 45;
		DrawLabel(col - 14.f, row, CounterColor, 0, "min/max");
		DrawLabel(col - 3.5f, row, CounterColor, 0, "Count/");
		DrawLabel(col + 1, row, HeaderColor, 0, sValueName);
		if (m_displayQuantity != STALL_TIME)
			DrawLabel(col + 5, row, HeaderColor, 0, " Function");
		else
			DrawLabel(col + 5, row, HeaderColor, 0, " Function (Action)");
	}
	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
}  //mode
	#endif
}

static const char* BeatifyProfilerThreadName(const char* inputName)
{
	static CryFixedStringT<256> str;
	const char* job = strstr(inputName, "JobSystem_Worker_");
	if (job)
	{
		str.Format("Job %s", job + sizeof("JobSystem_Worker_") - 1);
	}
	else
	{
		str = inputName;
	}

	return str.c_str();
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderProfiler(CFrameProfiler* pProfiler, int level, float col, float row, bool bExtended, bool bSelected)
{
	assert(pProfiler);
	char szText[128];
	char szThreadName[256];
	//  float HeaderColor[4] = { 1,1,0,1 };
	float ValueColor[4] = { 0, 1, 0, 1 };
	float ThreadColor[4] = { 0.8f, 0.8f, 0.2f, 1 };
	float CounterColor[4] = { 0, 0.8f, 1, 1 };
	float TextColor[4] = { 1, 1, 1, 1 };
	float SelectedColor[4] = { 1, 0, 0, 1 };
	float GraphColor[4] = { 1, 0.3f, 0, 1 };

	float colTextOfs = 5.0f + 4 * gEnv->IsDedicated();
	float colThreadfs = -11.0f + 4 * gEnv->IsDedicated();
	float colCountOfs = -5.0f - 3 * gEnv->IsDedicated();
	float glow = 0;

	const char* sValueFormat = "%4.2f";
	if (m_bMemoryProfiling)
	{
		sValueFormat = "%6.f";
		colTextOfs += 2;
	}

	if (!bExtended)
	{
		col += level;
		// Simple info.
		float value = pProfiler->m_displayedValue;
		float variance = pProfiler->m_variance;
		CalculateColor(value, variance, ValueColor, glow);

		if (bSelected)
		{
			memcpy(ValueColor, SelectedColor, sizeof(ValueColor));
			glow = 0;
		}
		else if (m_pGraphProfiler == pProfiler)
		{
			memcpy(ValueColor, GraphColor, sizeof(ValueColor));
			glow = 0;
		}

		cry_sprintf(szText, sValueFormat, value);
		DrawLabel(col, row, ValueColor, 0, szText);

		int cave = pProfiler->m_count.Average();
		if (cave > 1)
		{
			cry_sprintf(szText, "%6d/", cave);
			DrawLabel(col + colCountOfs, row, CounterColor, 0, szText);
		}

		if (m_displayQuantity == TOTAL_TIME && pProfiler->m_bHaveChildren)
		{
			cry_strcpy(szText, pProfiler->m_bExpended ? "-" : "+");
		}
		else
		{
			cry_strcpy(szText, "");
		}
		cry_strcat(szText, pProfiler->m_name);
		if (m_displayQuantity == STALL_TIME && pProfiler->m_stallCause)
		{
			char buf[128];
			cry_sprintf(buf, " (%s)", pProfiler->m_stallCause);
			cry_strcat(szText, buf);
		}

		cry_sprintf(szThreadName, "[%.8s]", BeatifyProfilerThreadName(GetProfilerThreadName(pProfiler)));

		DrawLabel(col + colThreadfs, row, ThreadColor, glow, szThreadName);
		DrawLabel(col + colTextOfs, row, ValueColor, glow, szText);

		// Render min/max values
		float tmin = 0, tmax = 0;
		if (m_displayQuantity == TOTAL_TIME)
		{
			tmin = pProfiler->m_totalTime.Min();
			tmax = pProfiler->m_totalTime.Max();
		}
		else if (m_displayQuantity == SELF_TIME)
		{
			tmin = pProfiler->m_selfTime.Min();
			tmax = pProfiler->m_selfTime.Max();
		}
		else if (m_displayQuantity == COUNT_INFO)
		{
			tmin = (float)pProfiler->m_count.Min();
			tmax = (float)pProfiler->m_count.Max();
		}
		cry_sprintf(szText, "%4.2f/%4.2f", tmin, tmax);
		DrawLabel(col + colTextOfs - 21, row, ValueColor, glow, szText, 0.8f);
	}
	else
	{
		// Extended display.
		if (bSelected)
		{
			memcpy(ValueColor, SelectedColor, sizeof(ValueColor));
			glow = 1;
		}

		float tmin, tmax, tave, tnow;
		int cmin, cmax, cave, cnow;
		if (m_displayQuantity == TOTAL_TIME_EXTENDED)
		{
			tmin = pProfiler->m_totalTime.Min();
			tmax = pProfiler->m_totalTime.Max();
			tave = pProfiler->m_totalTime.Average();
			tnow = pProfiler->m_selfTime.Average();
			//tnow = pProfiler->m_totalTime.Last();
		}
		else
		{
			tmin = pProfiler->m_selfTime.Min();
			tmax = pProfiler->m_selfTime.Max();
			tave = pProfiler->m_selfTime.Average();
			tnow = pProfiler->m_selfTime.Last();
		}

		cmin = pProfiler->m_count.Min();
		cmax = pProfiler->m_count.Max();
		cave = pProfiler->m_count.Average();
		cnow = pProfiler->m_count.Last();

		// Extensive info.
		cry_sprintf(szText, sValueFormat, tmax);
		DrawLabel(col, row, ValueColor, 0, szText);
		cry_sprintf(szText, sValueFormat, tmin);
		DrawLabel(col + 5, row, ValueColor, 0, szText);
		cry_sprintf(szText, sValueFormat, tave);
		DrawLabel(col + 10, row, ValueColor, 0, szText);
		cry_sprintf(szText, sValueFormat, tnow);
		DrawLabel(col + 15, row, ValueColor, 0, szText);

		if (cmax > 1)
		{
			cry_sprintf(szText, "/%d", cmax);
			DrawLabel(col + 3, row, CounterColor, 0, szText);
		}
		if (cmin > 1)
		{
			cry_sprintf(szText, "/%d", cmin);
			DrawLabel(col + 5 + 3, row, CounterColor, 0, szText);
		}
		if (cave > 1)
		{
			cry_sprintf(szText, "/%d", cave);
			DrawLabel(col + 10 + 3, row, CounterColor, 0, szText);
		}
		if (cnow > 1)
		{
			cry_sprintf(szText, "/%d", cnow);
			DrawLabel(col + 15 + 3, row, CounterColor, 0, szText);
		}

		if (pProfiler->m_bHaveChildren)
		{
			cry_strcpy(szText, pProfiler->m_bExpended ? "-" : "+");
		}
		else
		{
			cry_strcpy(szText, "");
		}
		cry_strcat(szText, GetFullName(pProfiler));

		DrawLabel(col + 20 + level, row, TextColor, glow, szText);
	}
}

//////////////////////////////////////////////////////////////////////////
float CFrameProfileSystem::RenderPeaks()
{
	char szText[128];
	char szWaitingText[128];
	float PeakColor[4] = { 1, 1, 1, 1 };
	float HotPeakColor[4] = { 1, 1, 1, 1 };
	float ColdPeakColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	float PeakHeaderColor[4] = { 0, 1, 1, 1 };
	float PeakCounterColor[4] = { 0, 0.8f, 1, 1 };
	float CounterColor[4] = { 0, 0.8f, 1, 1 };
	float PageFaultsColor[4] = { 1, 0.2f, 1, 1 };

	// changed from define to adjustable value
	float fHotToColdTime = profile_peak_display;
	float colPeaks = 8.0f;
	float row = 0.0f;
	float waitRow = 35.0f;
	float currentRow = row;

	float col = colPeaks;

	cry_sprintf(szText, "Latest Peaks");
	cry_sprintf(szWaitingText, "Latest Waiting Peaks");

	DrawLabel(colPeaks, row, PeakHeaderColor, 0, szText);
	DrawLabel(colPeaks, waitRow, PeakHeaderColor, 0, szWaitingText);

	float currTimeSec = gEnv->pTimer->TicksToSeconds(m_totalProfileTime);

	std::vector<SPeakRecord>& rPeaks = m_peaks;

	// Go through all peaks.
	for (int i = 0; i < (int)rPeaks.size(); i++)
	{
		SPeakRecord& peak = rPeaks[i];
		if (!peak.pProfiler)
			continue;

		currentRow = peak.waiting ? ++waitRow : ++row;

		float age = (currTimeSec - 1.0f) - peak.when;
		float ageFactor = age / fHotToColdTime;
		if (ageFactor < 0) ageFactor = 0;
		if (ageFactor > 1) ageFactor = 1;
		for (int k = 0; k < 4; k++)
			PeakColor[k] = ColdPeakColor[k] * ageFactor + HotPeakColor[k] * (1.0f - ageFactor);

		if (!m_bMemoryProfiling)
			cry_sprintf(szText, "%4.2fms", peak.peakValue);
		else
			cry_sprintf(szText, "%6.f", peak.peakValue);
		DrawLabel(col, currentRow, PeakColor, 0, szText);

		if (peak.count > 1)
		{
			for (int k = 0; k < 4; k++)
				PeakCounterColor[k] = CounterColor[k] * (1.0f - ageFactor);
			cry_sprintf(szText, "%4d/", peak.count);
			DrawLabel(col - 3, currentRow, PeakCounterColor, 0, szText);
		}
		if (peak.pageFaults > 0)
		{
			for (int k = 0; k < 4; k++)
				PeakCounterColor[k] = PageFaultsColor[k] * (1.0f - ageFactor);
			cry_sprintf(szText, "(%4d)", peak.pageFaults);
			DrawLabel(col - 6, currentRow, PeakCounterColor, 0, szText);
		}

		cry_strcpy(szText, GetFullName(peak.pProfiler));

		DrawLabel(col + 5, currentRow, PeakColor, 0, szText);

		if (age > fHotToColdTime)
		{
			rPeaks.erase(m_peaks.begin() + i);
			i--;
		}
	}

	return currentRow;
}

void DrawTextLabel(IRenderer* pRenderer, float& x, float& y, float* pColor, float fFontScale, const char* format, ...)
{
	char buffer[512];
	va_list args;
	va_start(args, format);
	cry_vsprintf(buffer, format, args);
	va_end(args);

	IRenderAuxText::Draw2dLabel(x, y, fFontScale, pColor, false, "%s", buffer);
	y += FrameProfileRenderConstants::c_yStepSizeText;
}

void GetColor(float scale, float* pColor)
{
	if (scale <= 0.5f)
	{
		pColor[0] = 0;
		pColor[1] = 1;
		pColor[2] = 0;
		pColor[3] = 1;
	}
	else if (scale <= 0.75f)
	{
		pColor[0] = (scale - 0.5f) * 4.0f;
		pColor[1] = 1;
		pColor[2] = 0;
		pColor[3] = 1;
	}
	else if (scale <= 1.0f)
	{
		pColor[0] = 1;
		pColor[1] = 1 - (scale - 0.75f) * 4.0f;
		pColor[2] = 0;
		pColor[3] = 1;
	}
	else
	{
		float time(gEnv->pTimer->GetAsyncCurTime());
		float blink(sinf(time * 6.28f) * 0.5f + 0.5f);
		pColor[0] = 1;
		pColor[1] = blink;
		pColor[2] = blink;
		pColor[3] = 1;
	}
}

void DrawMeter(IRenderer* pRenderer, float& x, float& y, float scale, float screenWidth, float screenHeight, float targetBarWidth = 0.21f)
{
	IRenderAuxGeom* pAuxRenderer = pRenderer->GetIRenderAuxGeom();

	//Aux Render setup
	SAuxGeomRenderFlags oldFlags = pAuxRenderer->GetRenderFlags();

	SAuxGeomRenderFlags flags(e_Def2DPublicRenderflags);
	flags.SetDepthTestFlag(e_DepthTestOff);
	flags.SetDepthWriteFlag(e_DepthWriteOff);
	flags.SetCullMode(e_CullModeNone);
	flags.SetMode2D3DFlag(e_ModeUnit);
	pAuxRenderer->SetRenderFlags(flags);

	// draw frame for meter
	vtx_idx indLines[8] =
	{
		0, 1, 1, 2,
		2, 3, 3, 0
	};

	const float barWidth = targetBarWidth > 1.0f ? targetBarWidth / screenWidth : targetBarWidth;

	const float yellowStart = 0.5f * barWidth;
	const float redStart = 0.75f * barWidth;

	Vec3 frame[4] =
	{
		Vec3((x - 1) / screenWidth,      (y - 1) / screenHeight,                                                 0),
		Vec3(x / screenWidth + barWidth, (y - 1) / screenHeight,                                                 0),
		Vec3(x / screenWidth + barWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0),
		Vec3((x - 1) / screenWidth,      (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0)
	};

	pAuxRenderer->DrawLines(frame, 4, indLines, 8, ColorB(255, 255, 255, 255));

	// draw meter itself
	vtx_idx indTri[6] =
	{
		0, 1, 2,
		0, 2, 3
	};

	// green part (0.0 <= scale <= 0.5)
	{
		float lScale(max(min(scale, 0.5f), 0.0f));

		Vec3 bar[4] =
		{
			Vec3(x / screenWidth,                     y / screenHeight,                                                       0),
			Vec3(x / screenWidth + lScale * barWidth, y / screenHeight,                                                       0),
			Vec3(x / screenWidth + lScale * barWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0),
			Vec3(x / screenWidth,                     (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0)
		};
		pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, ColorB(0, 255, 0, 255));
	}

	// green to yellow part (0.5 < scale <= 0.75)
	if (scale > 0.5f)
	{
		float lScale(min(scale, 0.75f));

		Vec3 bar[4] =
		{
			Vec3(x / screenWidth + yellowStart,       y / screenHeight,                                                       0),
			Vec3(x / screenWidth + lScale * barWidth, y / screenHeight,                                                       0),
			Vec3(x / screenWidth + lScale * barWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0),
			Vec3(x / screenWidth + yellowStart,       (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0)
		};

		float color[4];
		GetColor(lScale, color);

		ColorB colSegStart(0, 255, 0, 255);
		ColorB colSegEnd((uint8) (color[0] * 255), (uint8) (color[1] * 255), (uint8) (color[2] * 255), (uint8) (color[3] * 255));

		ColorB col[4] =
		{
			colSegStart,
			colSegEnd,
			colSegEnd,
			colSegStart
		};

		pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, col);
	}

	// yellow to red part (0.75 < scale <= 1.0)
	if (scale > 0.75f)
	{
		float lScale(min(scale, 1.0f));

		Vec3 bar[4] =
		{
			Vec3(x / screenWidth + redStart,          y / screenHeight,                                                       0),
			Vec3(x / screenWidth + lScale * barWidth, y / screenHeight,                                                       0),
			Vec3(x / screenWidth + lScale * barWidth, (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0),
			Vec3(x / screenWidth + redStart,          (y + FrameProfileRenderConstants::c_yStepSizeTextMeter) / screenHeight, 0)
		};

		float color[4];
		GetColor(lScale, color);

		ColorB colSegStart(255, 255, 0, 255);
		ColorB colSegEnd((uint8) (color[0] * 255), (uint8) (color[1] * 255), (uint8) (color[2] * 255), (uint8) (color[3] * 255));

		ColorB col[4] =
		{
			colSegStart,
			colSegEnd,
			colSegEnd,
			colSegStart
		};

		pAuxRenderer->DrawTriangles(bar, 4, indTri, 6, col);
	}

	y += FrameProfileRenderConstants::c_yStepSizeTextMeter;

	//restore Aux Render setup
	pAuxRenderer->SetRenderFlags(oldFlags);
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::DrawGraph()
{
	if (!m_pRenderer)
		return;

	// Layout:
	//  - Repeated X times
	//	 -------------------------------------------------------------
	//  |													Safe Area														|
	//	 -------------------------------------------------------------
	//	 -----------------------     ---------------------------------
	//	|												|		|																	|
	//  |			  Text Area			  |		|						Graph Area						|
	//	|												|		|																	|
	//	 -----------------------     ---------------------------------
	//

	const float VALUE_EPSILON = 0.000001f;

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	if (!m_ThreadFrameStats)
		return;

	JobManager::CWorkerFrameStats& rWorkerStatsInput = *m_ThreadFrameStats;
	#endif

	// UI item layout information
	const float cWorkerGraphScale = 180.f;     // Worker Graph displays the last X frames
	const float cTextAreaWidth = 220.f;        // Absolute Text Area width

	// Do not render in the top X% of rt top area to allow space for the r_DisplayInfo 1 profiler
	const float cTopSafeArea = 0.13f;

	// UI Control colour items
	float labelColor[4] = { 1.f, 1.f, 1.f, 1.f };
	float labelColDarkGreen[4] = { 0, 0.6f, 0.2f, 1.f };
	float labelColRed[4] = { 1.f, 0, 0, 1.f };
	float labelColorSuspended[4] = { 0.25f, 0.25f, 0.25f, 1.f };
	ColorF graphColor(0, 0.85, 0, 1);

	IRenderAuxGeom* pRenderAux = m_pRenderer->GetIRenderAuxGeom();

	// RT Area
	const float nRtWidth  = float(pRenderAux->GetCamera().GetViewSurfaceX());
	const float nRtHeight = float(pRenderAux->GetCamera().GetViewSurfaceZ());

	// Calculate overscan adjustment (for those elements that do not obey it)(see r_OverScanBoarder)
	Vec2 overscanBorder = Vec2(0.0f, 0.0f);
	gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, overscanBorder);
	//Vec2 overscanBorder = *(Vec2*)gEnv->pRenderer->EF_Query(EFQ_OverscanBorders, );

	const int fOverscanAdjX = (int)(overscanBorder.x * (float)nRtWidth);
	const int fOverscanAdjY = (int)(overscanBorder.y * (float)nRtHeight);

	// Surface Area
	const float nSafeAreaY = nRtHeight * cTopSafeArea;
	const float nSurfaceWidth = nRtWidth - fOverscanAdjX;
	const float nSurfaceHeight = nRtHeight - nSafeAreaY - fOverscanAdjX;

	// Calculate worker graph dimensions
	const float nTextAreaWidth = cTextAreaWidth * FrameProfileRenderConstants::c_yScale;
	const float nGraphStartX = nTextAreaWidth + nRtWidth * 0.015f * FrameProfileRenderConstants::c_yScale; // Fixed gap of 1.5% of RT width

	const int nGraphWidth = (int)(nSurfaceWidth - nGraphStartX - nSurfaceWidth * 0.05f * FrameProfileRenderConstants::c_yScale);   // Add a 5% of RT width gap to the right RT edge

	const float nTextAreaWidthOvrScnAdj = nTextAreaWidth + fOverscanAdjX;
	const float nGraphStartXOvrScnAdj = nGraphStartX + fOverscanAdjX;

	// Absolute coordinates tracker
	float x = 0;
	float y = 0;

	//*******************************
	// WORKER UTILIZATION INFORMATION
	//*******************************

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	const uint32 cNumWorkers = rWorkerStatsInput.numWorkers;   // Should be input variable
	const float cSamplePeriode = (float)rWorkerStatsInput.nSamplePeriod;

	// Absolute coordinates tracker
	x = 0;
	y = nRtHeight * cTopSafeArea;   // Start after top safe area in Y

	// Ensure worker graph size
	if (cNumWorkers != m_timeGraphWorkers.size())
		m_timeGraphWorkers.resize(cNumWorkers);

	// Draw common worker information
	DrawTextLabel(m_pRenderer, x, y, labelColDarkGreen, FrameProfileRenderConstants::c_fontScale, "Worker Utilization:");
	x += 5;   // Indent
	DrawTextLabel(m_pRenderer, x, y, labelColor, FrameProfileRenderConstants::c_fontScale, "Sample Time: %4.2fms:", cSamplePeriode * 0.001f);
	x -= 5;

	y += FrameProfileRenderConstants::c_yStepSizeText;   // Gap

	// Draw information about each active worker
	for (uint32 i = 0; i < cNumWorkers; ++i)
	{
		std::vector<unsigned char>& rTimeGraphWorker = m_timeGraphWorkers[i];

		// Ensure space in time graph tracker
		if (nGraphWidth + 1 != rTimeGraphWorker.size())
			rTimeGraphWorker.resize(nGraphWidth + 1);

		// Worker information
		const float nExecutionPeriodMs = (float)rWorkerStatsInput.workerStats[i].nExecutionPeriod * 0.001f;
		const float nIdleTime = (rWorkerStatsInput.nSamplePeriod > rWorkerStatsInput.workerStats[i].nExecutionPeriod) ? (float)(rWorkerStatsInput.nSamplePeriod - rWorkerStatsInput.workerStats[i].nExecutionPeriod) * 0.001f : 0.0f;
		const float nWorkloadPerJob = rWorkerStatsInput.workerStats[i].nNumJobsExecuted != 0 ? rWorkerStatsInput.workerStats[i].nUtilPerc / (float)rWorkerStatsInput.workerStats[i].nNumJobsExecuted : 0;

		// Initial data
		float yStart = y;
		float* pInfoLabelCol = labelColor;

		//Draw worker summary control
		DrawTextLabel(m_pRenderer, x, y, labelColor, FrameProfileRenderConstants::c_fontScale, "Worker %d:", i);

		x += 5;  // Indent
		DrawTextLabel(m_pRenderer, x, y, pInfoLabelCol, FrameProfileRenderConstants::c_fontScale, "Work Time: %05.2fms, Idle Time: %05.2fms", nExecutionPeriodMs, nIdleTime);
		DrawTextLabel(m_pRenderer, x, y, pInfoLabelCol, FrameProfileRenderConstants::c_fontScale, "Workload: %05.2f%%, Per Job: %05.2f%%, Jobs: %04i", rWorkerStatsInput.workerStats[i].nUtilPerc, nWorkloadPerJob, rWorkerStatsInput.workerStats[i].nNumJobsExecuted);
		x += fOverscanAdjX;
		y += fOverscanAdjY + 5.f;   // Push the meter down a further 5 for correct look
		DrawMeter(m_pRenderer, x, y, nExecutionPeriodMs * (1.f / 60.f), nRtWidth, nRtHeight, nTextAreaWidthOvrScnAdj - x);
		x -= fOverscanAdjX;
		y -= fOverscanAdjY;

		// Draw meter resolution
		const float quarterStepX = (nTextAreaWidth - 20.f * FrameProfileRenderConstants::c_yScale) * 0.25f;

		float charPosX = x + quarterStepX + 2.f;   // Hand tweaked char positions relative to meter
		float charPosY = y - 1;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.75f, "15");

		charPosX += quarterStepX + 2.f;
		charPosY = y;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.8f, "30ms");

		charPosX += quarterStepX + 4.f;
		charPosY = y - 1;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.75f, "45");

		charPosX += quarterStepX + 4.f;
		charPosY = y;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.8f, "60ms");
		x -= 5;   // Undo Indent

		// Draw worker graph
		const float graphHeight = y - yStart;
		rTimeGraphWorker[m_nWorkerGraphCurPos] = (unsigned char)clamp_tpl((255.f - (nExecutionPeriodMs * (1.f / 60.f) * 255.f)), 0.f, 255.f); // Convert from [0.f,1.f] to [0,255] range
		m_pRenderer->Graph(&rTimeGraphWorker[0], (int)(x + nGraphStartXOvrScnAdj), (int)(yStart + fOverscanAdjY), nGraphWidth, (int)graphHeight, m_nWorkerGraphCurPos, 2, 0, graphColor, 0);

		// Draw time graph indicator
		charPosX = x + nGraphStartX + (float)m_nWorkerGraphCurPos - 1.f;
		charPosY = y - 5.f;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, pInfoLabelCol, FrameProfileRenderConstants::c_fontScale, "|");

		// Draw graph resolution information
		const float quarterStepY = graphHeight * 0.25f;
		yStart = y - graphHeight - 7.f;   // Hand tweaked for correct char position relative to graph curve

		charPosX = x + (x + nGraphStartX + nGraphWidth + 5.f * FrameProfileRenderConstants::c_yScale);
		charPosY = yStart;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "60ms");

		charPosY = yStart + quarterStepY;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.7f, "45");

		charPosY = yStart + quarterStepY * 2.f;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "30ms");

		charPosY = yStart + quarterStepY * 3.f;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.7f, "15");

		y += FrameProfileRenderConstants::c_yNextControlGap;
	}
	#endif

	//************************
	// Page Fault Information
	//************************
	// Display values via a log10 graph
	#if CRY_PLATFORM_WINDOWS
	{
		if (nGraphWidth + 1 != m_timeGraphPageFault.size())
			m_timeGraphPageFault.resize(nGraphWidth + 1);

		const float cLogGraphScale = 5.f;
		const float yStart = y;
		DrawTextLabel(m_pRenderer, x, y, labelColDarkGreen, FrameProfileRenderConstants::c_fontScale, "Page Faults:");

		x += 5;
		DrawTextLabel(m_pRenderer, x, y, labelColor, FrameProfileRenderConstants::c_fontScale, "Faults: %6i  ", m_nPagesFaultsLastFrame);

		// Display values via a log10 graph
		// log10(10,000) = 4, because 10^4 = 10,000
		float logValue = 0;

		if (m_nPagesFaultsLastFrame)
			logValue = log10((float)m_nPagesFaultsLastFrame + VALUE_EPSILON);

		x += fOverscanAdjX;
		y += fOverscanAdjY + 5; // Push the meter down a further 5 for correct look
		DrawMeter(m_pRenderer, x, y, logValue * (100.f / cLogGraphScale) * 0.01f, nRtWidth, nRtHeight, nTextAreaWidthOvrScnAdj - x);
		x -= fOverscanAdjX;
		y -= fOverscanAdjY;
		x -= 5;

		float charPosX = x + nTextAreaWidth - 20.f * FrameProfileRenderConstants::c_yScale;
		float charPosY = y;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale, "10000");

		m_timeGraphPageFault[m_nWorkerGraphCurPos] = (unsigned char)(255.f - clamp_tpl(logValue * (255.f / cLogGraphScale), 0.f, 255.f));

		y += FrameProfileRenderConstants::c_yStepSizeText * 2.f; // Add some extra height to the graph
		const float graphHeight = y - yStart;
		m_pRenderer->Graph(&m_timeGraphPageFault[0], (int)(x + nGraphStartXOvrScnAdj), (int)(yStart + fOverscanAdjY), nGraphWidth, (int)graphHeight, m_nWorkerGraphCurPos, 2, 0, graphColor, 0);

		// Draw time graph indicator
		charPosX = x + nGraphStartX + (float)m_nWorkerGraphCurPos - 1.f;
		charPosY = y - 5.f;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale, "|");

		// Draw graph resolution information
		const float stepY = graphHeight / 5.f;
		charPosX = x + nGraphStartX + nGraphWidth + 5.f * FrameProfileRenderConstants::c_yScale;  // Use FrameProfileRenderConstants::c_yScale to adjust for larger font size
		charPosY = yStart;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%i", 10000);
		charPosY += stepY;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%i", 1000);
		charPosY += stepY;
		DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%i", 100);
		/*	charPosY += stepY;
		   DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%i", 0);*/

		y += FrameProfileRenderConstants::c_yNextControlGap;
	}
	#endif

	//************************
	// Frame Time Information
	//************************
	{
		if (nGraphWidth + 1 != m_timeGraphFrameTime.size())
			m_timeGraphFrameTime.resize(nGraphWidth + 1);

		if (m_displayQuantity != COUNT_INFO)
		{
			const float yStart = y;

			DrawTextLabel(m_pRenderer, x, y, labelColDarkGreen, FrameProfileRenderConstants::c_fontScale, "Frame Timings:");

			const float frameTime = m_frameTimeHistory.GetLast();
			const float framesPerSec = 1.0f / ((frameTime + VALUE_EPSILON) * 0.001f); // Convert ms to fps (1/ms = fps)
			const float framePercentage = frameTime * (1.f / 200.f);

			x += 5;
			DrawTextLabel(m_pRenderer, x, y, labelColor, FrameProfileRenderConstants::c_fontScale, "Time: %06.2fms", frameTime);

			x += fOverscanAdjX;
			y += fOverscanAdjY + 5; // Push the meter down a further 5 for correct look
			DrawMeter(m_pRenderer, x, y, framePercentage, nRtWidth, nRtHeight, nTextAreaWidthOvrScnAdj - x);
			x -= fOverscanAdjX;
			y -= fOverscanAdjY;

			float charPosX = x + nTextAreaWidth - 20.f * FrameProfileRenderConstants::c_yScale;
			float charPosY = y - 2.5f;
			DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.88f, "200ms");

			DrawTextLabel(m_pRenderer, x, y, labelColor, FrameProfileRenderConstants::c_fontScale, "Frames: %06.2ffps", framesPerSec);
			x += fOverscanAdjX;
			y += fOverscanAdjY + 5; // Push the meter down a further 5 for correct look
			DrawMeter(m_pRenderer, x, y, framesPerSec * 0.01f, nRtWidth, nRtHeight, nTextAreaWidthOvrScnAdj - x);
			x -= fOverscanAdjX;
			y -= fOverscanAdjY;

			charPosX = x + nTextAreaWidth - 20.f * FrameProfileRenderConstants::c_yScale;
			;
			charPosY = y;
			DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.88f, "100fps");
			x -= 5;

	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
			m_timeGraphFrameTime[m_nWorkerGraphCurPos] = (unsigned char)clamp_tpl((255.0f - (framePercentage * 100.f * 2.555f)), 0.f, 255.f);   // Convert into [0.f,100.f] and then to [0,255] range

			m_pRenderer->Graph(&m_timeGraphFrameTime[0], (int)(x + nGraphStartXOvrScnAdj), (int)(yStart + fOverscanAdjY), nGraphWidth, int(y - yStart), m_nWorkerGraphCurPos, 2, 0, graphColor, 0);

			// Draw time graph indicator
			charPosX = x + nGraphStartX + (float)m_nWorkerGraphCurPos - 1.f;
			charPosY = y - 5.f;
			DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale, "|");
	#endif

			// Draw graph resolution information
			charPosX = x + nGraphStartX + nGraphWidth + 5.f * FrameProfileRenderConstants::c_yScale;
			charPosY = yStart;
			DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale * 0.85f, "%ims", 200);

			y += FrameProfileRenderConstants::c_yNextControlGap;
		}
	}

	// Draw graph resolution
	float charPosX = x + nGraphStartX + nGraphWidth - 35.f * FrameProfileRenderConstants::c_yScale;  // Use FrameProfileRenderConstants::c_yScale to adjust for larger font size
	float charPosY = y - FrameProfileRenderConstants::c_yStepSizeText;
	DrawTextLabel(m_pRenderer, charPosX, charPosY, labelColor, FrameProfileRenderConstants::c_fontScale, "%d frames", nGraphWidth);

	// Advance
	if (!m_bCollectionPaused)
	{
	#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
		++m_nWorkerGraphCurPos;

		if (m_nWorkerGraphCurPos > nGraphWidth)
			m_nWorkerGraphCurPos = 0;
	#endif
	}
}

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderHistograms()
{
	if (!m_pRenderer)
		return;

	ColorF HistColor(0, 1, 0, 1);

	// Draw histograms.
	int h = m_pRenderer->GetOverlayHeight();
	int w = m_pRenderer->GetOverlayWidth();

	int graphStyle = 2; // histogram.

	float fScale = 1.0f; // histogram.


	for (int i = 0; i < (int)m_displayedProfilers.size(); i++)
	{
		if (i > MAX_DISPLAY_ROWS)
		{
			break;
		}
		SProfilerDisplayInfo& dispInfo = m_displayedProfilers[i];
		CFrameProfilerGraph* pGraph = dispInfo.pProfiler->m_pGraph;
		if (!pGraph)
			continue;

		// Add a value to graph.
		pGraph->m_x = (int)(dispInfo.x + COL_DIFF_HISTOGRAMS * COL_SIZE);
		pGraph->m_y = (int)(dispInfo.y);
		if (pGraph->m_y >= h)
			continue;
		// Render histogram.
		m_pRenderer->Graph(&pGraph->m_data[0], pGraph->m_x, pGraph->m_y, pGraph->m_width, pGraph->m_height, m_histogramsCurrPos, graphStyle, 0, HistColor, fScale);
	}
	if (!m_bCollectionPaused)
	{
		m_histogramsCurrPos++;
		if (m_histogramsCurrPos >= m_histogramsMaxPos)
			m_histogramsCurrPos = 0;
	}
}

void CFrameProfileSystem::RenderSubSystems(float col, float row)
{
	char szWorkText[128];
	char szWaitText[128];
	float HeaderColor[4] = { 1, 1, 0, 1 };
	float ValueColor[4] = { 0, 1, 0, 1 };
	float CounterColor[4] = { 0, 0.8f, 1, 1 };

	float colPercOfs = -3.0f;
	float colTextOfs = 9.0f;

	m_baseY += 40;

	// Go through all profilers.
	for (int i = 0; i < PROFILE_LAST_SUBSYSTEM; i++)
	{
		// Simple info.
		float workValue = m_subsystems[i].selfTime;
		float waitValue = m_subsystems[i].waitTime;
		m_subsystems[i].selfTime = 0;
		m_subsystems[i].waitTime = 0;
		const char* sName = m_subsystems[i].name;
		if (!sName)
			continue;

		if (!m_bMemoryProfiling)
		{
			cry_sprintf(szWorkText, "%4.2fms", workValue);
			cry_sprintf(szWaitText, "%4.2fms", waitValue);
		}
		else
		{
			cry_sprintf(szWorkText, "%6.0f", workValue);
			cry_sprintf(szWaitText, "%6.0f", waitValue);
		}

		if (workValue > m_subsystems[i].budgetTime)
		{
			DrawLabel(col, row, HeaderColor, 0, szWorkText);
			DrawLabel(col + colTextOfs, row, HeaderColor, 0, szWaitText);
			DrawLabel(col + 2 * colTextOfs, row, HeaderColor, 0, sName);
		}
		else
		{
			DrawLabel(col, row, ValueColor, 0, szWorkText);
			DrawLabel(col + colTextOfs, row, ValueColor, 0, szWaitText);
			DrawLabel(col + 2 * colTextOfs, row, ValueColor, 0, sName);
		}

		row += 1.0f;
	}
	m_baseY -= 40;
}

	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
		#pragma pack(push,1)
const struct PEHeader
{
	DWORD                 signature;
	IMAGE_FILE_HEADER     _head;
	IMAGE_OPTIONAL_HEADER opt_head;
	IMAGE_SECTION_HEADER* section_header;  // actual number in NumberOfSections
};
		#pragma pack(pop)
	#endif

//////////////////////////////////////////////////////////////////////////
void CFrameProfileSystem::RenderMemoryInfo()
{

	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_DURANGO

	m_pRenderer = gEnv->pRenderer;
	if (!m_pRenderer)
		return;

	m_baseY = 0;
	m_textModeBaseExtra = -1;
	ROW_SIZE = 11;
	COL_SIZE = 11;

	float col = 1;
	float row = 1;

	float HeaderColor[4] = { 0.3f, 1, 1, 1 };
	float ModuleColor[4] = { 1, 1, 1, 1 };
	float StaticColor[4] = { 1, 1, 1, 1 };
	float NumColor[4] = { 1, 0, 1, 1 };
	float TotalColor[4] = { 1, 1, 1, 1 };

	char szText[128];
	float fLabelScale = 1.1f;

	ILog* pLog = gEnv->pLog;
	//////////////////////////////////////////////////////////////////////////
	// Show memory usage.
	//////////////////////////////////////////////////////////////////////////
	uint64 memUsage = 0;//CryMemoryGetAllocatedSize();
	int64 totalAll = 0;
	int luaMemUsage = gEnv->pScriptSystem->GetScriptAllocSize();

	row++; // reserve for static.
	row++;
	row++;

	cry_sprintf(szText, "Lua Allocated Memory: %d KB", luaMemUsage / 1024);
	DrawLabel(col, row++, HeaderColor, 0, szText, fLabelScale);

	if (m_bLogMemoryInfo) pLog->Log(szText);
	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	float col1 = col + 20;
	float col2 = col1 + 20;
		#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	float col3 = col2;
		#else
	float col3 = col2 + 12;
		#endif
	float col4 = col3 + 12;

	DrawLabel(col, row++, HeaderColor, 0, "-----------------------------------------------------------------------------------------------------------------------------------", fLabelScale);
	DrawLabel(col, row, HeaderColor, 0, "Module", fLabelScale);
	DrawLabel(col1, row, HeaderColor, 0, "Dynamic(MB)", fLabelScale);
		#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
	DrawLabel(col2, row, HeaderColor, 0, "Static(KB)", fLabelScale);
		#endif
	DrawLabel(col3, row, HeaderColor, 0, "Num Allocs", fLabelScale);
	DrawLabel(col4, row, HeaderColor, 0, "Total Allocs(KB)", fLabelScale);
	float col5 = col4 + 20;
	DrawLabel(col5, row, HeaderColor, 0, "Total Wasted(KB)", fLabelScale);
	int totalUsedInModulesStatic = 0;

	row++;

	uint64 totalUsedInModules = 0;
	int countedMemoryModules = 0;
	uint64 totalAllocatedInModules = 0;
	int totalNumAllocsInModules = 0;

	const std::vector<string>& szModules = GetModuleNames();
	const int numModules = szModules.size();

	for (int i = 0; i < numModules; i++)
	{
		#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
		PEHeader pe_header;
		PEHeader* header = 0;
		#endif

		CryModuleMemoryInfo memInfo;
		memset(&memInfo, 0, sizeof(memInfo));

		uint32 moduleStaticSize = 0;
		#ifndef _LIB
		const char* szModule = szModules[i];
			#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
		char path[_MAX_PATH];
		cry_sprintf(path, "./%s", szModule);
				#if CRY_PLATFORM_ANDROID
		HMODULE hModule = dlopen(path, RTLD_LAZY);
				#else
		HMODULE hModule = dlopen(path, RTLD_LAZY | RTLD_NOLOAD);
				#endif
			#else
		HMODULE hModule = GetModuleHandle(szModule);
			#endif
		if (!hModule)
		{
			continue;
		}
		else
		{

			//totalStatic += me.modBaseSize;
			typedef void (* PFN_MODULEMEMORY)(CryModuleMemoryInfo*);
			#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
			PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY)dlsym(hModule, "CryModuleGetMemoryInfo");
			#else
			PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = (PFN_MODULEMEMORY)::GetProcAddress(hModule, "CryModuleGetMemoryInfo");
			#endif
			if (!fpCryModuleGetAllocatedMemory)
				continue;
		#else // _LIB
		typedef void (* PFN_MODULEMEMORY)(CryModuleMemoryInfo*);
		PFN_MODULEMEMORY fpCryModuleGetAllocatedMemory = &CryModuleGetMemoryInfo;
		const char* szModule = "Unknown";
		#endif

		#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)

			header = &pe_header;

			#if CRY_PLATFORM_WINDOWS && !defined(_LIB)
			const IMAGE_DOS_HEADER* dos_head = (IMAGE_DOS_HEADER*)hModule;
			if (dos_head->e_magic != IMAGE_DOS_SIGNATURE)
			{
				// Wrong pointer, not to PE header.
				continue;
			}
			header = (PEHeader*)(const void*)((char*)dos_head + dos_head->e_lfanew);
			#endif
		#endif

		#ifndef _LIB
			fpCryModuleGetAllocatedMemory(&memInfo);
		}
		#else
			fpCryModuleGetAllocatedMemory(&memInfo);
		#endif

		uint64 usedInModule = memInfo.allocated - memInfo.freed;
		#ifndef _LIB

			#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
		PREFAST_SUPPRESS_WARNING(28199);
		PREFAST_SUPPRESS_WARNING(6001) moduleStaticSize = header->opt_head.SizeOfInitializedData + header->opt_head.SizeOfUninitializedData + header->opt_head.SizeOfCode + header->opt_head.SizeOfHeaders;
			#else
		moduleStaticSize = 0;
			#endif
		if (numModules - 1 == i)
		{
			usedInModule = 46 * 1024 * 1024;
			moduleStaticSize = 0;
		}

		#endif

		totalNumAllocsInModules += memInfo.num_allocations;
		totalAllocatedInModules += memInfo.allocated;
		totalUsedInModules += usedInModule;
		countedMemoryModules++;
		memUsage += usedInModule + moduleStaticSize;
		#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
		totalUsedInModulesStatic += moduleStaticSize;
		#endif
		string szModuleName = PathUtil::GetFileName(szModule);
		
		cry_sprintf(szText, "%.19s", szModuleName.c_str());
		DrawLabel(col, row, ModuleColor, 0, szText, fLabelScale);
		cry_sprintf(szText, "%9.2f", usedInModule / 1024.0f / 1024.0f);
		DrawLabel(col1, row, StaticColor, 0, szText, fLabelScale);
		#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
		cry_sprintf(szText, "%d", moduleStaticSize / 1024);
		DrawLabel(col2, row, StaticColor, 0, szText, fLabelScale);
		#endif
		cry_sprintf(szText, "%d",  memInfo.num_allocations);
		DrawLabel(col3, row, NumColor, 0, szText, fLabelScale);
		cry_sprintf(szText, "%" PRIu64, memInfo.allocated / 1024u);

		DrawLabel(col4, row, TotalColor, 0, szText, fLabelScale);
		#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
		cry_sprintf(szText, "%" PRIu64, (memInfo.allocated - memInfo.requested) / 1024u);
		DrawLabel(col5, row, TotalColor, 0, szText, fLabelScale);
		if (m_bLogMemoryInfo)
		{
			pLog->Log("    %20s | Alloc: %6d Kb  |  Num: %7d  |  TotalAlloc: %8I64d KB  | StaticTotal: %6d KB  | Code: %6d KB |  Init. Data: %6d KB  |  Uninit. Data: %6d KB | %6d | %6d/%6d",
			          szModule,
			          usedInModule / 1024, memInfo.num_allocations, memInfo.allocated / 1024,
			          moduleStaticSize / 1024,
			          header->opt_head.SizeOfCode / 1024,
			          header->opt_head.SizeOfInitializedData / 1024,
			          header->opt_head.SizeOfUninitializedData / 1024,
			          (uint32)memInfo.CryString_allocated / 1024, (uint32)memInfo.STL_allocated / 1024, (uint32)memInfo.STL_wasted / 1024);
		}
		#else
		if (m_bLogMemoryInfo)
		{
			pLog->Log("    %20s | Alloc: %6d Kb  |  Num: %7d  |  TotalAlloc: %" PRIu64 "KB",
			          szModule,
			          usedInModule / 1024, memInfo.num_allocations, memInfo.allocated / 1024u);
		}
		#endif
		row++;
	}

	DrawLabel(col, row++, HeaderColor, 0, "-----------------------------------------------------------------------------------------------------------------------------------", fLabelScale);
	cry_sprintf(szText, "Sum %d Modules", countedMemoryModules);
	DrawLabel(col, row, HeaderColor, 0, szText, fLabelScale);
	cry_sprintf(szText, "%d", totalUsedInModules / 1024 / 1024);
	DrawLabel(col1, row, HeaderColor, 0, szText, fLabelScale);
		#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
	cry_sprintf(szText, "%d", totalUsedInModulesStatic / 1024);
	DrawLabel(col2, row, StaticColor, 0, szText, fLabelScale);
		#endif
	cry_sprintf(szText, "%d", totalNumAllocsInModules);
	DrawLabel(col3, row, NumColor, 0, szText, fLabelScale);

	cry_sprintf(szText, "%" PRIu64, totalAllocatedInModules / 1024u);

	DrawLabel(col4, row, TotalColor, 0, szText, fLabelScale);
	row++;

		#if CRY_PLATFORM_WINDOWS
	//	col = 0;;

	PROCESS_MEMORY_COUNTERS ProcessMemoryCounters = { sizeof(ProcessMemoryCounters) };
	GetProcessMemoryInfo(GetCurrentProcess(), &ProcessMemoryCounters, sizeof(ProcessMemoryCounters));
	SIZE_T WorkingSetSize = ProcessMemoryCounters.WorkingSetSize;
	SIZE_T QuotaPagedPoolUsage = ProcessMemoryCounters.QuotaPagedPoolUsage;
	SIZE_T PagefileUsage = ProcessMemoryCounters.PagefileUsage;
	SIZE_T PageFaultCount = ProcessMemoryCounters.PageFaultCount;

	cry_sprintf(szText, "WindowsInfo: PagefileUsage: %u WorkingSetSize: %u, QuotaPagedPoolUsage: %u PageFaultCount: %u\n",
	            (uint)PagefileUsage / 1024,
	            (uint)WorkingSetSize / 1024,
	            (uint)QuotaPagedPoolUsage / 1024,
	            (uint) PageFaultCount);

	DrawLabel(col, row++, HeaderColor, 0, "-----------------------------------------------------------------------------------------------------------------------------------", fLabelScale);
	//	cry_sprintf( szText,"WindowsInfo",countedMemoryModules );
	DrawLabel(col, row, HeaderColor, 0, szText, fLabelScale);
		#endif
		#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
	if (m_bLogMemoryInfo)
		pLog->Log("Sum of %d Modules %6d Kb  (Static: %6d Kb)  (Num: %8d) (TotalAlloc: %8I64d KB)", countedMemoryModules, totalUsedInModules / 1024,
		          totalUsedInModulesStatic / 1024, totalNumAllocsInModules, totalAllocatedInModules / 1024);
		#endif

	int memUsageInMB_SysCopyMeshes = 0;
	int memUsageInMB_SysCopyTextures = 0;
	m_pRenderer->EF_Query(EFQ_Alloc_APIMesh, memUsageInMB_SysCopyMeshes);
	m_pRenderer->EF_Query(EFQ_Alloc_APITextures, memUsageInMB_SysCopyTextures);

	totalAll += memUsage;
	totalAll += memUsageInMB_SysCopyMeshes;
	totalAll += memUsageInMB_SysCopyTextures;

	cry_sprintf(szText, "Total Allocated Memory: %" PRId64 " KB (DirectX Textures: %d KB, VB: %d Kb)", totalAll / 1024, memUsageInMB_SysCopyTextures / 1024, memUsageInMB_SysCopyMeshes / 1024);

	DrawLabel(col, 1, HeaderColor, 0, szText, fLabelScale);

	m_bLogMemoryInfo = false;

	#endif //#if CRY_PLATFORM_WINDOWS
}
	#undef VARIANCE_MULTIPLIER

#endif // USE_FRAME_PROFILER

#if defined(JOBMANAGER_SUPPORT_FRAMEPROFILER)
	#undef COL_DIFF_JOBS
	#undef ROW_DIFF_BLOCKING_HEADER
	#undef ROW_DIFF_BLOCKING
	#undef ROW_DIFF_THREAD_HEADER
	#undef ROW_DIFF_THREAD
#endif
#undef COL_DIFF_HISTOGRAMS
