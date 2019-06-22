// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProfilingRenderer.h"
#include "CryProfilingSystem.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySystem/ITextModeConsole.h>
#include <CrySystem/ConsoleRegistration.h>
#include <CryInput/IInput.h>
#include <CryCore/RingBuffer.h>
#include <CryThreading/IThreadManager.h>

int   CProfilingRenderer::profile = 0;
float CProfilingRenderer::profile_row = 0;
float CProfilingRenderer::profile_col = 0;
int   CProfilingRenderer::profile_log = 0;
float CProfilingRenderer::profile_peak_display = 0;
float CProfilingRenderer::profile_min_display_ms = 0;
int   CProfilingRenderer::profile_hide_waits = 0;

static CProfilingRenderer* s_pRendererInstance = nullptr;
void CProfilingRenderer::ChangeListenStatusCommand(ICVar*)
{
	static bool registered = false;
	if(s_pRendererInstance && GetISystem()->GetLegacyProfilerInterface())
	{
		if (profile || profile_log)
		{
			if(!registered)
			{
				GetISystem()->GetLegacyProfilerInterface()->AddFrameListener(s_pRendererInstance);
				registered = true;
			}
		}
		else
		{
			GetISystem()->GetLegacyProfilerInterface()->RemoveFrameListener(s_pRendererInstance);
			registered = false;
		}
	}
}

CProfilingRenderer::CProfilingRenderer()
	: m_isCollectionPaused(true)
	, m_displayQuantity(EDisplayQuantity::SELF_VALUE)
	, m_selectedRow(0)
{
	CRY_ASSERT(s_pRendererInstance == nullptr);
	s_pRendererInstance = this;
}

CProfilingRenderer::~CProfilingRenderer()
{
	CRY_ASSERT(s_pRendererInstance == this);
	s_pRendererInstance = nullptr;
	if(GetISystem()->GetLegacyProfilerInterface())
		GetISystem()->GetLegacyProfilerInterface()->RemoveFrameListener(this);
}

void CProfilingRenderer::RegisterCVars()
{
	REGISTER_CVAR_CB(profile, 0, 0,
		"Allows CPU profiling\n"
		"Usage: profile #\n"
		"Where # sets the profiling to:\n"
		"	0: Don't show\n"
		"	1: Self Value\n"
		"	2: Total Value\n"
		"	3: Counts\n"
		"	4: Peaks\n"
		"	5: Per Subsystem Info\n"
		"	6: Per Thread Info\n"
		"	7: Minimal View\n"
		"Default is 0 (off)",
		&ChangeListenStatusCommand
		);

	REGISTER_CVAR(profile_row, 5, 0,
		"Starting row for profile display");
	REGISTER_CVAR(profile_col, 50, 0,
		"Starting column for profile display");
	REGISTER_CVAR(profile_peak_display, 8.0f, 0,
		"For how long to display peak values in seconds");
	REGISTER_CVAR_CB(profile_log, 0, 0, "Writes profiler data into log. 'profile_log 1' to output average values. 'profile_log 2' to output min/max values.", &ChangeListenStatusCommand);
	REGISTER_CVAR(profile_hide_waits, 1, 0, "Removes sections flagged as 'waiting' from the profile X displays.");
	REGISTER_CVAR(profile_min_display_ms, 0.01f, 0, "Minimum duration for functions to be displayed in the profile view.");
}

void CProfilingRenderer::OnFrameEnd(TTime timestamp, ILegacyProfiler* pProfSystem)
{
	if (CProfilingRenderer::profile_log)
	{
		CryLogAlways("==================== Start Profiler Frame %d, Time %.2f ======================", gEnv->nMainFrameID, gEnv->pTimer->GetFrameTime(ITimer::ETIMER_GAME) * 1000);
		CryLogAlways("|\tCount\t|\tSelf\t|\tTotal\t|\tModule\t|");
		CryLogAlways("|\t____\t|\t_____\t|\t_____\t|\t_____\t|");

		int logType = CProfilingRenderer::profile_log;

		for (SProfilingSectionTracker* pTracker : *pProfSystem->GetActiveTrackers())
		{
			if (logType == 1)
			{
				uint count = pos_round(pTracker->count.Average());
				float fTotalTimeMs = pTracker->totalValue.Average();
				float fSelfTimeMs = pTracker->selfValue.Average();

				CryLogAlways("|\t%d\t|\t%.2f\t|\t%.2f\t|\t%s\t|\t %s", count, fSelfTimeMs, fTotalTimeMs, pProfSystem->GetModuleName(pTracker), GetNameWithThread(pTracker));
			}
			else if (logType == 2)
			{
				int   c_min  = pos_round(pTracker->count.Min());
				int   c_max  = pos_round(pTracker->count.Max());
				float t1_min = pTracker->totalValue.Min();
				float t1_max = pTracker->totalValue.Max();
				float t0_min = pTracker->selfValue.Min();
				float t0_max = pTracker->selfValue.Max();
				CryLogAlways("|\t%d/%d\t|\t%.2f/%.2f\t|\t%.2f/%.2f\t|\t%s\t|\t %s", c_min, c_max, t0_min, t0_max, t1_min, t1_max, pProfSystem->GetModuleName(pTracker), GetNameWithThread(pTracker));
			}
		}

		CryLogAlways("======================= End Profiler Frame %d ==========================", gEnv->pRenderer->GetFrameID(false));
		
		// Reset logging, so we only log one frame.
		CProfilingRenderer::profile_log = 0;
		ChangeListenStatusCommand(nullptr);
	}
}

namespace
{
	const int   c_mainHeaderRows = 4;
	const float ROW_SIZE = 10;
	const float COL_SIZE = 11;

	const float MainHeaderColor[4] = { 0, 1, 1, 1 };
	const float HeaderColor[4] = { 1, 1, 0, 1 };
	const float CounterColor[4] = { 0, 0.8f, 1, 1 };
	const float PausedColor[4] = { 1, 0.3f, 0, 1 };
	const float ValueColor[4] = { 0, 1, 0, 1 };
	const float ThreadColor[4] = { 0.8f, 0.8f, 0.2f, 1 };
	const float TextColor[4] = { 1, 1, 1, 1 };
}

//////////////////////////////////////////////////////////////////////////
void CProfilingRenderer::DrawLabel(float col, float row, const float* fColor, float glow, const char* szText, float fScale)
{
	if (gEnv->pRenderer)
	{
		ColorF color;
		float scale = fScale * 1.3f;
		int flags = eDrawText_2D | eDrawText_FixedSize | eDrawText_Monospace;
		color[0] = fColor[0];
		color[1] = fColor[1];
		color[2] = fColor[2];

		// coordinate transform from char grid to pixels
		col += profile_col;
		col *= COL_SIZE;
		row += profile_row;
		row *= ROW_SIZE;

		if (glow > 0.1f)
		{
			color[3] = glow;
			IRenderAuxText::DrawText(Vec3(col, row, 0.5f), scale, color, flags, szText);
		}

		color[3] = fColor[3];
		IRenderAuxText::DrawText(Vec3(col, row, 0.5f), scale, color, flags, szText);
	}

	if (ITextModeConsole* pTC = gEnv->pSystem->GetITextModeConsole())
	{
		pTC->PutText((int)col, (int)row, szText);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProfilingRenderer::DrawRect(float col1, float row1, float col2, float row2, const float* fColor)
{
	float x1 = (col1 + profile_col) * COL_SIZE;
	float x2 = (col2 + profile_col) * COL_SIZE;
	float y1 = (row1 + 0.5f + profile_row) * ROW_SIZE - 2;
	float y2 = (row2 + 0.5f + profile_row) * ROW_SIZE;

	ColorB col((uint8)(fColor[0] * 255.0f), (uint8)(fColor[1] * 255.0f), (uint8)(fColor[2] * 255.0f), (uint8)(fColor[3] * 255.0f));

	IRenderAuxGeom* pAux = gEnv->pRenderer->GetIRenderAuxGeom();
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
		factor = (difference / value) * 2;

	return factor;
}

inline void CalculateColor(float value, float variance, float* outColor, float& glow)
{
	const float ColdColor[4] = { 0.15f, 0.9f, 0.15f, 1 };
	const float HotColor[4] = { 1, 1, 1, 1 };

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
SProfilingSectionTracker* CProfilingRenderer::GetSelectedTracker(ILegacyProfiler* pProfSystem) const
{
	if ((*pProfSystem->GetActiveTrackers()).empty())
		return nullptr;

	return (*pProfSystem->GetActiveTrackers())[m_selectedRow];
}

const char* GetThreadName(SProfilingSectionTracker* pTracker)
{
	const char* szThreadName = gEnv->pThreadManager->GetThreadName(pTracker->threadId);
	if (CRY_PROFILER_COMBINE_JOB_TRACKERS && pTracker->threadId == CRY_PROFILER_JOB_THREAD_ID)
	{
		return "Job Worker";
	}
	else if (szThreadName && szThreadName[0])
	{
		return szThreadName;
	}
	else
	{
		static char szThreadId[16];
		cry_sprintf(szThreadId, "TID_%" PRI_THREADID, pTracker->threadId);
		return szThreadId;
	}
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

const char* CProfilingRenderer::GetNameWithThread(SProfilingSectionTracker* pTracker) const
{
	static char sNameBuffer[256];
	cry_sprintf(sNameBuffer, pTracker->pDescription->szEventname);

	if (pTracker->threadId != gEnv->mMainThreadId)
	{
		strcat(sNameBuffer, "@");
		strcat(sNameBuffer, GetThreadName(pTracker));
	}

	return sNameBuffer;
}

//////////////////////////////////////////////////////////////////////////
void CProfilingRenderer::Render(CCryProfilingSystem* pProfSystem)
{
	if (!Enabled())
		return;

	m_isCollectionPaused = pProfSystem->IsPaused();
	m_displayQuantity = (EDisplayQuantity)crymath::clamp(profile - 1, 0, DISPLAY_SETTINGS_COUNT - 1);

	RenderMainHeader(pProfSystem);

	switch (m_displayQuantity)
	{
	case CProfilingRenderer::MINIMAL:
		return; // just the header
	case CProfilingRenderer::SELF_VALUE:
	case CProfilingRenderer::TOTAL_VALUE:
	case CProfilingRenderer::COUNTS:
		RenderTrackerValues(pProfSystem);
		break;
	case CProfilingRenderer::PEAKS:
		RenderPeaks(pProfSystem);
		break;
	case CProfilingRenderer::SUBSYSTEM_INFO:
		RenderSubSystems(pProfSystem);
		break;
	case CProfilingRenderer::THREAD_INFO:
		RenderThreadInfo(pProfSystem);
		break;
	default:
		assert(false);
	}
}

void CProfilingRenderer::RenderMainHeader(CCryProfilingSystem* pProfSystem)
{
	char szText[256];

	float row = 0;

	if (m_isCollectionPaused)
		DrawLabel(0, row, PausedColor, 0, "============ PAUSED ============");
	++row;

	switch (m_displayQuantity)
	{
	case MINIMAL:
		cry_strcpy(szText, "Basic Overview");
		break;
	case SELF_VALUE:
		cry_strcpy(szText, "Profile Mode: Self");
		break;
	case TOTAL_VALUE:
		cry_strcpy(szText, "Profile Mode: Total");
		break;
	case COUNTS:
		cry_strcpy(szText, "Profile Mode: Call Counts");
		break;
	case PEAKS:
		cry_strcpy(szText, "Latest Peaks");
		break;
	case SUBSYSTEM_INFO:
		cry_strcpy(szText, "Average per Subsystem");
		break;
	case THREAD_INFO:
		cry_strcpy(szText, "Average per Thread");
		break;
	default:
		cry_strcpy(szText, "<Invalid>");
	}
	
	ICVar* pModeVar = gEnv->pConsole->GetCVar("profile_value");
	if (pModeVar)
	{
		if (pModeVar->GetIVal() == CCryProfilingSystem::TIME)
			cry_strcat(szText, ", Time in ms");
		else if(pModeVar->GetIVal() == CCryProfilingSystem::MEMORY)
			cry_strcat(szText, ", Memory in kB");
		else
			cry_strcat(szText, " <Unknown>");
	}
	else
		cry_strcat(szText, " <Unknown>");

	cry_strcat(szText, " (Hide Waits: ");
	cry_strcat(szText, profile_hide_waits ? "true" : "false");
	cry_strcat(szText, ")");

	ICVar* pVerbosityVar = gEnv->pConsole->GetCVar("profile_verbosity");
	if (pVerbosityVar)
	{
		cry_strcat(szText, " (Verbosity: ");
		char verbosity[16];
		cry_sprintf(verbosity, "%d", pVerbosityVar->GetIVal());
		cry_strcat(szText, verbosity);
		cry_strcat(szText, ")");
	}

	DrawLabel(0, row, (m_isCollectionPaused ? PausedColor : MainHeaderColor), 0, szText);
	++row;

	cry_sprintf(szText, "%6.2fms", gEnv->pTimer->GetFrameTime(ITimer::ETIMER_GAME) * 1000);
	DrawLabel(0, row, ValueColor, 0, szText);
	DrawLabel(7, row, TextColor, 0, "Frame time");

	cry_sprintf(szText, "%4.0f | ", pProfSystem->PageFaultsPerFrame().Latest());
	DrawLabel(20, row, ValueColor, 0, szText);
	cry_sprintf(szText, "%4.0f", pProfSystem->PageFaultsPerFrame().Average());
	DrawLabel(25, row, ValueColor, 0, szText);
	DrawLabel(32, row, TextColor, 0, "Page Faults (current | average)");
	++row;

	cry_sprintf(szText, "%6.2fms", pProfSystem->GetAverageProfilingTimeCost());
	DrawLabel(0, row, ValueColor, 0, szText);
	DrawLabel(7, row, TextColor, 0, "Profiling overhead per Frame (avg)");
}

//////////////////////////////////////////////////////////////////////////
namespace
{
	const int   c_maxDisplayRows = 50;
	const float columnValueOffset = 8;
	//4 values: min, average, max, current
	const float columnCountOffset = 4 * columnValueOffset;
	const float columnThreadOffset = columnCountOffset + 5;
	const float columnNameOffset = columnThreadOffset + 10;
};

void CProfilingRenderer::RenderTrackerValues(ILegacyProfiler* pProfSystem)
{
	float row = c_mainHeaderRows;
	// Header
	DrawLabel(0 * columnValueOffset, row, HeaderColor, 0, "Min");
	DrawLabel(1 * columnValueOffset, row, HeaderColor, 0, "Average");
	DrawLabel(2 * columnValueOffset, row, HeaderColor, 0, "Max");
	DrawLabel(3 * columnValueOffset, row, HeaderColor, 0, "Current");
	DrawLabel(columnCountOffset, row, HeaderColor, 0, "Count");
	DrawLabel(columnNameOffset, row, HeaderColor, 0, "Name");
	DrawLabel(columnThreadOffset, row, HeaderColor, 0, "Thread");
	++row;
	// ~Header

	const auto& allTrackers = *pProfSystem->GetActiveTrackers();
	ILegacyProfiler::TrackerList displayedTrackers;
	displayedTrackers.reserve(allTrackers.size());

	if (m_displayQuantity == TOTAL_VALUE)
	{
		for (SProfilingSectionTracker* pTracker : allTrackers)
		{
			if (abs(pTracker->totalValue.Average()) >= profile_min_display_ms && !(pTracker->pDescription->isWaiting && profile_hide_waits))
				displayedTrackers.push_back(pTracker);
		}
	}
	else
	{
		for (SProfilingSectionTracker* pTracker : allTrackers)
		{
			if (abs(pTracker->selfValue.Average()) >= profile_min_display_ms && !(pTracker->pDescription->isWaiting && profile_hide_waits))
				displayedTrackers.push_back(pTracker);
		}
	}

	if (m_displayQuantity == SELF_VALUE)
		std::sort(displayedTrackers.begin(), displayedTrackers.end(),
			[](const SProfilingSectionTracker* t1, const SProfilingSectionTracker* t2)
			{ return abs(t1->selfValue.Latest()) > abs(t2->selfValue.Latest()); }
		);
	else if (m_displayQuantity == TOTAL_VALUE)
		std::sort(displayedTrackers.begin(), displayedTrackers.end(),
			[](const SProfilingSectionTracker* t1, const SProfilingSectionTracker* t2)
			{ return abs(t1->totalValue.Latest()) > abs(t2->totalValue.Latest()); }
		);
	else if (m_displayQuantity == COUNTS)
		std::sort(displayedTrackers.begin(), displayedTrackers.end(),
			[](const SProfilingSectionTracker* t1, const SProfilingSectionTracker* t2)
			{ return t1->count.Latest() > t2->count.Latest(); }
		);

	m_selectedRow = crymath::clamp<int>(m_selectedRow, 0, displayedTrackers.size() - 1);
	const int startIndex = max(0, m_selectedRow - c_maxDisplayRows / 2);
	const int lastIndex = min<int>(startIndex + c_maxDisplayRows, displayedTrackers.size());

	for (int i = startIndex; i < lastIndex; i++)
	{
		RenderTrackerValues(displayedTrackers[i], row, (i == m_selectedRow));
		++row;
	}
}

//////////////////////////////////////////////////////////////////////////
template <typename T>
static void ExtractValues(CSamplerStats<T>& value, float& vmin, float& vavg, float& vmax, float& vnow)
{
	vmin = value.Min();
	vavg = value.Average();
	vmax = value.Max();
	vnow = value.Latest();
}

void CProfilingRenderer::RenderTrackerValues(SProfilingSectionTracker* pTracker, float row, bool selected)
{
	assert(pTracker);
	char szText[128];
	float InstanceValueColor[4];
	memcpy(InstanceValueColor, ValueColor, sizeof(InstanceValueColor));

	// target values
	const char* szValueFormat = "%8.2f";
	float vmin = -1, vavg = -1, vmax = -1, vnow = -1;
	if (m_displayQuantity == TOTAL_VALUE)
		ExtractValues(pTracker->totalValue, vmin, vavg, vmax, vnow);
	else if (m_displayQuantity == SELF_VALUE)
		ExtractValues(pTracker->selfValue, vmin, vavg, vmax, vnow);
	else if (m_displayQuantity == COUNTS)
	{
		ExtractValues(pTracker->count, vmin, vavg, vmax, vnow);
		szValueFormat = "%8.0f";
	}
	else
		assert(false);

	float glow = 1;
	float variance = pTracker->selfValue.Variance();
	CalculateColor(vnow, variance, InstanceValueColor, glow);

	if (selected)
	{
		DrawLabel(-2, row, TextColor, glow, "->");
		DrawRect(0, row, columnNameOffset + 50, row + 1, TextColor);
	}

	cry_sprintf(szText, szValueFormat, vmin);
	DrawLabel(0 * columnValueOffset, row, InstanceValueColor, glow, szText);
	cry_sprintf(szText, szValueFormat, vavg);
	DrawLabel(1 * columnValueOffset, row, InstanceValueColor, glow, szText);
	cry_sprintf(szText, szValueFormat, vmax);
	DrawLabel(2 * columnValueOffset, row, InstanceValueColor, glow, szText);
	cry_sprintf(szText, szValueFormat, vnow);
	DrawLabel(3 * columnValueOffset, row, InstanceValueColor, glow, szText);

	cry_sprintf(szText, "%5.0f", pTracker->count.Latest());
	DrawLabel(columnCountOffset, row, CounterColor, 0, szText);

	cry_sprintf(szText, "[%.10s]", BeatifyProfilerThreadName(GetThreadName(pTracker)));
	DrawLabel(columnThreadOffset, row, ThreadColor, glow, szText);
	DrawLabel(columnNameOffset, row, TextColor, glow, pTracker->pDescription->szEventname);
}

//////////////////////////////////////////////////////////////////////////
void CProfilingRenderer::RenderPeaks(ILegacyProfiler* pProfSystem)
{
	const float HotPeakColor[4] = { 1, 1, 1, 1 };
	const float ColdPeakColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	const float PeakCountOffset = 7;
	const float PeakNameOffset = 12;

	float row = c_mainHeaderRows;
	// Header
	DrawLabel(0, row, HeaderColor, row, "Value");
	DrawLabel(PeakCountOffset, row, HeaderColor, 0, "Count");
	DrawLabel(PeakNameOffset, row, HeaderColor, 0, "Name");
	++row;
	// ~Header

	char szText[32];

	float currTimeSec = gEnv->pTimer->GetAsyncTime().GetSeconds();

	const auto& peaks = *pProfSystem->GetPeakRecords();
	for (int i = 0; i < peaks.size(); i++)
	{
		const SPeakRecord& peak = peaks[i];

		float age = currTimeSec - peak.timeSeconds;
		if (age >= profile_peak_display)
			continue;
		if (peak.pTracker->pDescription->isWaiting && profile_hide_waits)
			continue;

		float ageFactor = crymath::clamp<float>(age / profile_peak_display, 0, 1);
		++row;

		float PeakColor[4];
		for (int k = 0; k < 4; k++)
			PeakColor[k] = ColdPeakColor[k] * ageFactor + HotPeakColor[k] * (1.0f - ageFactor);

		cry_sprintf(szText, "%4.2f", peak.peakValue);
		DrawLabel(0, row, PeakColor, 0, szText);

		float PeakCounterColor[4];
		for (int k = 0; k < 4; k++)
			PeakCounterColor[k] = CounterColor[k] * (1.0f - ageFactor);
		cry_sprintf(szText, "%4d", peak.count);
		DrawLabel(PeakCountOffset, row, PeakCounterColor, 0, szText);

		DrawLabel(PeakNameOffset, row, PeakColor, 0, GetNameWithThread(peak.pTracker));
	}
}

void CProfilingRenderer::RenderSubSystems(CCryProfilingSystem* pProfSystem)
{
	const float PercentOffset = 8;
	const float NameOffset = 13;
	char szText[32];

	float row = c_mainHeaderRows;
	++row; // empty header

	float times[EProfiledSubsystem::PROFILE_LAST_SUBSYSTEM] = { 0 };
	float waitingTime = 0;

	for (SProfilingSectionTracker* pTracker : *pProfSystem->GetActiveTrackers())
	{
		if (pTracker->pDescription->isWaiting)
			waitingTime += pTracker->selfValue.Average();
		else
			times[pTracker->pDescription->subsystem] += pTracker->selfValue.Average();
	}

	const float frameTime = gEnv->pTimer->GetFrameTime(ITimer::ETIMER_GAME) * 1000;
	for (int subsystem = 0; subsystem < EProfiledSubsystem::PROFILE_LAST_SUBSYSTEM; ++subsystem)
	{
		cry_sprintf(szText, "%6.2f", times[subsystem]);
		DrawLabel(0, row + subsystem, ValueColor, 0, szText);

		cry_sprintf(szText, "%5.1f%%", (times[subsystem] / frameTime) * 100);
		DrawLabel(PercentOffset, row + subsystem, ValueColor, 0, szText);

		DrawLabel(NameOffset, row + subsystem, TextColor, 0, pProfSystem->GetModuleName((EProfiledSubsystem)subsystem));
	}

	cry_sprintf(szText, "%6.2f", waitingTime);
	DrawLabel(0, row + EProfiledSubsystem::PROFILE_LAST_SUBSYSTEM, ValueColor, 0, szText);
	DrawLabel(NameOffset, row + EProfiledSubsystem::PROFILE_LAST_SUBSYSTEM, TextColor, 0, "Waiting (All Systems)");
}

struct SThreadProfileInfo
{
	float activeValue = 0;
	float waitValue = 0;
	uint sectionsTracked = 0;
};

void CProfilingRenderer::RenderThreadInfo(ILegacyProfiler* pProfSystem)
{
	const float ActiveTimeOffset = 0;
	const float ActiveTimePercentOffset = 8 + ActiveTimeOffset;
	const float WaitTimeOffset = 5 + ActiveTimePercentOffset;
	const float WaitTimePercentOffset = 8 + WaitTimeOffset;
	const float NameOffset = 5 + WaitTimePercentOffset;
	char szText[32];
	std::map<threadID, SThreadProfileInfo> threadInfoMap;

	float row = c_mainHeaderRows;
	// Header
	DrawLabel(ActiveTimeOffset, row, HeaderColor, 0, "Active");
	//DrawLabel(ActiveTimePercentOffset, row, HeaderColor, 0, "%");
	DrawLabel(WaitTimeOffset, row, HeaderColor, 0, "Waiting");
	//DrawLabel(WaitTimePercentOffset, row, HeaderColor, 0, "%");
	DrawLabel(NameOffset, row, HeaderColor, 0, "Thread Name");
	++row;
	// ~Header

	for (SProfilingSectionTracker* pTracker : *pProfSystem->GetActiveTrackers())
	{
		SThreadProfileInfo& info = threadInfoMap[pTracker->threadId];
		info.sectionsTracked += uint(pTracker->count.Latest());
		if (pTracker->pDescription->isWaiting)
			info.waitValue += pTracker->selfValue.Latest();
		else
			info.activeValue += pTracker->selfValue.Latest();
	}

	const float frameTime = gEnv->pTimer->GetFrameTime(ITimer::ETIMER_GAME) * 1000;
	for (const auto& infoPair : threadInfoMap)
	{
		const SThreadProfileInfo& info = infoPair.second;
		cry_sprintf(szText, "%5.2f", info.activeValue);
		DrawLabel(ActiveTimeOffset, row, ValueColor, 0, szText);

		cry_sprintf(szText, "%4.1f%%", (info.activeValue / frameTime) * 100);
		DrawLabel(ActiveTimePercentOffset, row, ValueColor, 0, szText);

		cry_sprintf(szText, "%5.2f", info.waitValue);
		DrawLabel(WaitTimeOffset, row, ValueColor, 0, szText);

		cry_sprintf(szText, "%4.1f%%", (info.waitValue / frameTime) * 100);
		DrawLabel(WaitTimePercentOffset, row, ValueColor, 0, szText);

		const char* szThreadName = gEnv->pThreadManager->GetThreadName(infoPair.first);
		if (szThreadName && szThreadName[0])
		{
			DrawLabel(NameOffset, row, TextColor, 0, szThreadName);
		}
		else
		{
			cry_sprintf(szText, "<Unnamed Thread, id = %" PRI_THREADID ">", infoPair.first);
			DrawLabel(NameOffset, row, TextColor, 0, szText);
		}

		++row;
	}
}

bool CProfilingRenderer::OnInputEvent(const SInputEvent& event)
{
	if (event.deviceType == eIDT_Keyboard && event.state == eIS_Down)
	{
		switch (event.keyId)
		{
		case eKI_Up:
			--m_selectedRow;
			break;
		case eKI_Down:
			++m_selectedRow;
			break;
		}
	}

	return false;
}

