// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "FlashPlayerInstance.h"
#include "../System.h"
#include <CrySystem/IConsole.h>

// flash player implementation via Scaleform's GFx
#ifdef INCLUDE_SCALEFORM_SDK

	#include <CryString/CryPath.h>

	#include "GAllocatorCryMem.h"
	#include "SharedStates.h"
	#include "GFxVideoWrapper.h"
	#include "GFxVideoSoundCrySound.h"
	#include "GImageInfo_Impl.h"
	#include "GTexture_Impl.h"

	#if defined(GFX_AMP_SERVER)
		#include "GFxAmpServer.h"
	#endif

	#include <CryString/StringUtils.h>              // stristr()

	#if defined(ENABLE_FLASH_INFO)
static int ms_sys_flash_info = 0;
	#endif

	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_ORBIS || CRY_PLATFORM_DURANGO || CRY_PLATFORM_MAC || CRY_PLATFORM_LINUX
enum { DEFAULT_VA_SPACE_IN_KB = 48 * 1024 };
	#else
enum { DEFAULT_VA_SPACE_IN_KB = 32 * 1024 };
	#endif

//////////////////////////////////////////////////////////////////////////
// profile timer

	#if defined(ENABLE_FLASH_INFO) || defined(ENABLE_LW_PROFILERS)
struct FlashTimer
{
public:
	static uint64 GetTicks()
	{
		uint64 ticks;
		QueryPerformanceCounter((LARGE_INTEGER*) &ticks);
		return ticks;
	}
	static float TicksToSeconds(const uint64& ticks)
	{
		return (float) ((double) ticks / (double) ms_frequency);
	}

private:
	static const uint64 ms_frequency;

private:
	static uint64 InitFrequency()
	{
		uint64 freq = 0;
		QueryPerformanceFrequency((LARGE_INTEGER*) &freq);
		return freq;
	}

private:
	FlashTimer() {}
	~FlashTimer() {}
};

const uint64 FlashTimer::ms_frequency = InitFrequency();
	#endif // #if defined(ENABLE_FLASH_INFO) || defined(ENABLE_LW_PROFILERS)

//////////////////////////////////////////////////////////////////////////
// IFlashPlayer instance profiler helpers

	#if defined(ENABLE_FLASH_INFO)
struct SFlashPeak
{
	SFlashPeak(float timeRecorded, const char* pDisplayInfo)
		: m_timeRecorded(timeRecorded)
		, m_displayInfo(pDisplayInfo)
	{
	}

	float  m_timeRecorded;
	string m_displayInfo;
};

class CFlashPeakHistory
{
public:
	typedef std::vector<SFlashPeak> FlashPeakHistory;

public:
	CFlashPeakHistory(size_t maxPeakEntries, float timePeakExpires)
		: m_writeProtected(false)
		, mc_maxPeakEntries(maxPeakEntries)
		, mc_timePeakExpires(timePeakExpires)
		, m_history()
	{
		m_history.reserve(mc_maxPeakEntries);
	}

	void Add(const SFlashPeak& peak)
	{
		/*	if (!m_writeProtected) //This isn't thread safe so disabling for now
		   {
		    if (m_history.size() >= mc_maxPeakEntries)
		      m_history.pop_back();

		    m_history.insert(m_history.begin(), peak);
		   }*/
	}

	void ToggleWriteProtection()
	{
		m_writeProtected = !m_writeProtected;
	}

	void UpdateHistory(float curTime)
	{
		FlashPeakHistory::iterator it(m_history.begin());
		for (; it != m_history.end(); )
		{
			const SFlashPeak& peak(*it);
			if (curTime - peak.m_timeRecorded > mc_timePeakExpires && !m_writeProtected)
				it = m_history.erase(it);
			else
				++it;
		}
	}

	float GetTimePeakExpires() const
	{
		return mc_timePeakExpires;
	}

	const FlashPeakHistory& GetHistory() const
	{
		return m_history;
	}

private:
	bool             m_writeProtected;
	const size_t     mc_maxPeakEntries;
	const float      mc_timePeakExpires;
	FlashPeakHistory m_history;
};

static CFlashPeakHistory s_flashPeakHistory(10, 5.0f);

enum EFlashFunctionID
{
	eFncAdvance,
	eFncDisplay,
	eFncSetVar,
	eFncGetVar,
	eFncIsAvailable,
	eFncInvoke,

	eNumFncIDs
};

inline static const char* GetFlashFunctionName(EFlashFunctionID functionID)
{
	switch (functionID)
	{
	case eFncAdvance:
		return "Advance";
	case eFncDisplay:
		return "Display";
	case eFncSetVar:
		return "SetVar";
	case eFncGetVar:
		return "GetVar";
	case eFncIsAvailable:
		return "IsAvailable";
	case eFncInvoke:
		return "Invoke";
	default:
		return "Unknown";
	}
}

template<typename T, unsigned int HistogramSize>
struct SFlashProfilerHistogram
{
	SFlashProfilerHistogram()
		: m_curIdx(0)
	{
		for (unsigned int i(0); i < HistogramSize; ++i)
			m_hist[i] = 0;
	}

	void Advance()
	{
		m_curIdx = (m_curIdx + 1) % HistogramSize;
		m_hist[m_curIdx] = 0;
	}

	void Add(T amount)
	{
		m_hist[m_curIdx] += amount;
	}

	T GetAvg() const
	{
		T sum(0);
		for (unsigned int i(0); i < HistogramSize; ++i)
			sum += m_hist[i];

		return sum / (T) HistogramSize;
	}

	T GetMin() const
	{
		T min(m_hist[0]);
		for (unsigned int i(1); i < HistogramSize; ++i)
			min = m_hist[i] < min ? m_hist[i] : min;

		return min;
	}

	T GetMax() const
	{
		T max(m_hist[0]);
		for (unsigned int i(1); i < HistogramSize; ++i)
			max = m_hist[i] > max ? m_hist[i] : max;

		return max;
	}

	T GetCur() const
	{
		return m_hist[m_curIdx];
	}

	T GetAt(unsigned int idx) const
	{
		return m_hist[idx];
	}

	unsigned int GetCurIdx() const
	{
		return m_curIdx;
	}

private:
	unsigned int m_curIdx;
	T            m_hist[HistogramSize];
};

struct SFlashProfilerData
{
	enum
	{
		HistogramSize = 64
	};

	typedef SFlashProfilerHistogram<unsigned int, HistogramSize> TotalCallHistogram;
	typedef SFlashProfilerHistogram<float, HistogramSize>        TotalTimeHistogram;
	typedef SFlashProfilerHistogram<unsigned int, HistogramSize> DrawCallHistogram;
	typedef SFlashProfilerHistogram<unsigned int, HistogramSize> TriCountHistogram;
	typedef SFlashProfilerHistogram<unsigned int, HistogramSize> LocklessDisparityHistogram;

	SFlashProfilerData()
		: m_viewExpanded(false)
		, m_preventFunctionExectution(false)
		, m_totalCalls()
		, m_totalTime()
		, m_totalTimeAllFuncs()
		, m_drawCalls()
		, m_triCount()
		//, m_curDrawCalls()
		//, m_curTriCount()
	{
		for (unsigned int i(0); i < eNumFncIDs + 1; ++i)
		{
			m_funcColorValue[i] = 0;
			m_funcColorValueVariance[i] = 0;
		}

		for (unsigned int i = 0; i < 2; ++i)
		{
			m_curDrawCalls[i] = 0;
			m_curTriCount[i] = 0;
		}
	}

	void Advance()
	{
		if (!ms_histWriteProtected)
		{
			for (unsigned int i(0); i < eNumFncIDs; ++i)
			{
				m_totalCalls[i].Advance();
				m_totalTime[i].Advance();
			}
			m_totalTimeAllFuncs.Advance();
			m_locklessDisparity.Advance();
			m_drawCalls.Advance();
			m_triCount.Advance();
		}
	}

	void UpdateDrawStats(unsigned int idx, bool write)
	{
		if (write)
		{
			m_drawCalls.Add(m_curDrawCalls[idx]);
			m_triCount.Add(m_curTriCount[idx]);
		}

		m_curDrawCalls[idx] = 0;
		m_curTriCount[idx] = 0;
	}

	static void AdvanceAllInsts()
	{
		if (!ms_histWriteProtected)
			ms_totalTimeAllInsts.Advance();
	}

	void AddTotalCalls(EFlashFunctionID functionID, unsigned int amount)
	{
		if (!ms_histWriteProtected)
			m_totalCalls[functionID].Add(amount);
	}

	void AddTotalTime(EFlashFunctionID functionID, float amount)
	{
		if (!ms_histWriteProtected)
		{
			m_totalTime[functionID].Add(amount);
			m_totalTimeAllFuncs.Add(amount);
			ms_totalTimeAllInsts.Add(amount);
		}
	}

	void AddLocklessDisparity()
	{
		if (!ms_histWriteProtected)
			m_locklessDisparity.Add(1);
	}

	void AddDrawCalls(unsigned int drawCalls, unsigned int idx)
	{
		m_curDrawCalls[idx] += drawCalls;
	}

	void AddTriCount(unsigned int triCount, unsigned int idx)
	{
		m_curTriCount[idx] += triCount;
	}

	const TotalCallHistogram& GetTotalCallsHisto(EFlashFunctionID functionID) const
	{
		return m_totalCalls[functionID];
	}

	const TotalTimeHistogram& GetTotalTimeHisto(EFlashFunctionID functionID) const
	{
		return m_totalTime[functionID];
	}

	const TotalTimeHistogram& GetTotalTimeHistoAllFuncs() const
	{
		return m_totalTimeAllFuncs;
	}

	const LocklessDisparityHistogram& GetLocklessDisparity() const
	{
		return m_locklessDisparity;
	}

	const DrawCallHistogram& GetDrawCallHisto() const
	{
		return m_drawCalls;
	}

	const TriCountHistogram& GetTriCountHisto()
	{
		return m_triCount;
	}

	static const TotalTimeHistogram& GetTotalTimeHistoAllInsts()
	{
		return ms_totalTimeAllInsts;
	}

	void SetViewExpanded(bool expanded)
	{
		m_viewExpanded = expanded;
	}

	bool IsViewExpanded() const
	{
		return m_viewExpanded;
	}

	void TogglePreventFunctionExecution()
	{
		m_preventFunctionExectution = !m_preventFunctionExectution;
	}

	bool PreventFunctionExectution() const
	{
		return m_preventFunctionExectution;
	}

	void SetColorValue(EFlashFunctionID functionID, float value)
	{
		if (!ms_histWriteProtected)
			m_funcColorValue[functionID] = value;
	}

	float GetColorValue(EFlashFunctionID functionID) const
	{
		return m_funcColorValue[functionID];
	}

	void SetColorValueVariance(EFlashFunctionID functionID, float value)
	{
		if (!ms_histWriteProtected)
			m_funcColorValueVariance[functionID] = value;
	}

	float GetColorValueVariance(EFlashFunctionID functionID) const
	{
		return m_funcColorValueVariance[functionID];
	}

	static void ToggleHistogramWriteProtection()
	{
		ms_histWriteProtected = !ms_histWriteProtected;
	}

	static bool HistogramWriteProtected()
	{
		return ms_histWriteProtected;
	}

	static void ToggleDisplayFrozen()
	{
		ms_displayFrozen = !ms_displayFrozen;
	}

	static void ResetDisplayFrozen()
	{
		ms_displayFrozen = false;
	}

	static bool DisplayFrozen()
	{
		return ms_displayFrozen;
	}

	static void ToggleShowStrippedPath()
	{
		ms_showStrippedPath = !ms_showStrippedPath;
	}

	static bool ShowStrippedPath()
	{
		return ms_showStrippedPath;
	}

private:
	static bool               ms_histWriteProtected;
	static bool               ms_displayFrozen;
	static bool               ms_showStrippedPath;
	static TotalTimeHistogram ms_totalTimeAllInsts;

private:
	bool                       m_viewExpanded;
	bool                       m_preventFunctionExectution;
	float                      m_funcColorValue[eNumFncIDs + 1];
	float                      m_funcColorValueVariance[eNumFncIDs + 1];
	TotalCallHistogram         m_totalCalls[eNumFncIDs];
	TotalTimeHistogram         m_totalTime[eNumFncIDs];
	TotalTimeHistogram         m_totalTimeAllFuncs;
	LocklessDisparityHistogram m_locklessDisparity;
	DrawCallHistogram          m_drawCalls;
	TriCountHistogram          m_triCount;
	unsigned int               m_curDrawCalls[2];
	unsigned int               m_curTriCount[2];
};

bool SFlashProfilerData::ms_histWriteProtected = false;
bool SFlashProfilerData::ms_displayFrozen = false;
bool SFlashProfilerData::ms_showStrippedPath = false;
SFlashProfilerData::TotalTimeHistogram SFlashProfilerData::ms_totalTimeAllInsts;

struct SPODVariant
{
	struct SFlashVarValueList
	{
		const SFlashVarValue* pArgs;
		unsigned int          numArgs;
	};

	union Data
	{
		bool                    b;

		int                     i;
		unsigned int            ui;

		double                  d;
		float                   f;

		const char*             pStr;
		const wchar_t*          pWstr;
		const void*             pVoid;

		EFlashVariableArrayType fvat;

		const SFlashVarValue*   pfvv;
		SFlashVarValueList      fvvl;
	};

	enum Type
	{
		eBool,

		eInt,
		eUInt,

		eDouble,
		eFloat,

		eConstStrPtr,
		eConstWstrPtr,
		eConstVoidPtr,

		eFlashVariableArrayType,

		eFlashVarValuePtr,
		eFlashVarValueList
	};

	SPODVariant(bool val)
		: type(eBool)
	{
		data.b = val;
	}
	SPODVariant(int val)
		: type(eInt)
	{
		data.i = val;
	}
	SPODVariant(unsigned int val)
		: type(eUInt)
	{
		data.ui = val;
	}
	SPODVariant(double val)
		: type(eDouble)
	{
		data.d = val;
	}
	SPODVariant(float val)
		: type(eFloat)
	{
		data.f = val;
	}
	SPODVariant(const char* val)
		: type(eConstStrPtr)
	{
		data.pStr = val;
	}
	SPODVariant(const wchar_t* val)
		: type(eConstWstrPtr)
	{
		data.pWstr = val;
	}
	SPODVariant(const void* val)
		: type(eConstVoidPtr)
	{
		data.pVoid = val;
	}
	SPODVariant(const EFlashVariableArrayType& val)
		: type(eFlashVariableArrayType)
	{
		data.fvat = val;
	}
	SPODVariant(const SFlashVarValue* val)
		: type(eFlashVarValuePtr)
	{
		data.pfvv = val;
	}
	SPODVariant(const SFlashVarValueList& val)
		: type(eFlashVarValueList)
	{
		data.fvvl = val;
	}

	Type type;
	Data data;
};

class CFlashFunctionProfilerLog
{
public:
	static CFlashFunctionProfilerLog& GetAccess()
	{
		static CFlashFunctionProfilerLog s_instance;
		return s_instance;
	}

	void Enable(bool enabled, int curFrame)
	{
		if (!enabled && m_file)
		{
			fclose(m_file);
			m_file = 0;
		}
		else if (enabled && !m_file)
		{
			m_file = fopen("flash.log", "wb");
		}
		m_enabled = enabled;

		if (m_curFrame != curFrame && m_file != 0)
		{
			char frameMsg[512];
			cry_sprintf(frameMsg, ">>>>>>>> Frame: %.10d <<<<<<<<", curFrame);
			fwrite("\n", 1, 1, m_file);
			fwrite(frameMsg, 1, strlen(frameMsg), m_file);
			fwrite("\n\n", 1, 2, m_file);
		}
		m_curFrame = curFrame;
	}

	bool IsEnabled() const
	{
		return m_enabled;
	}

	void Log(const char* pMsg)
	{
		if (m_file && pMsg)
		{
			fwrite(pMsg, 1, strlen(pMsg), m_file);
			fwrite("\n", 1, 1, m_file);
		}
	}

private:
	CFlashFunctionProfilerLog()
		: m_enabled(false)
		, m_curFrame(0)
		, m_file(0)
	{
	}

	~CFlashFunctionProfilerLog()
	{
	}

	bool         m_enabled;
	unsigned int m_curFrame;
	FILE*        m_file;
};

class CFlashFunctionProfiler
{
public:
	CFlashFunctionProfiler(float peakTolerance, EFlashFunctionID functionID, int numFuncArgs, const SPODVariant* pFuncArgs,
	                       const char* pCustomFuncName, SFlashProfilerData* pProfilerData, const char* pFlashFilePath)
		: m_funcTime(0)
		, m_peakTolerance(peakTolerance)
		, m_functionID(functionID)
		, m_numFuncArgs(numFuncArgs)
		, m_pFuncArgs(pFuncArgs)
		, m_pCustomFuncName(pCustomFuncName)
		, m_pProfilerData(pProfilerData)
		, m_pFlashFilePath(pFlashFilePath)
	{
	}

	void ProfileEnter()
	{
		m_funcTime = FlashTimer::GetTicks();
	}

	void ProfilerLeave()
	{
		m_funcTime = FlashTimer::GetTicks() - m_funcTime;

		float prevTotalTimeMs(m_pProfilerData->GetTotalTimeHisto(m_functionID).GetCur());
		float curTotalTimeMs(FlashTimer::TicksToSeconds(m_funcTime) * 1000.0f);

		m_pProfilerData->AddTotalCalls(m_functionID, 1);
		m_pProfilerData->AddTotalTime(m_functionID, curTotalTimeMs);

		bool isPeak(curTotalTimeMs - prevTotalTimeMs > m_peakTolerance && (ms_peakFuncExcludeMask & (1 << m_functionID)) == 0);
		bool reqLog(CFlashFunctionProfilerLog::GetAccess().IsEnabled());
		if (isPeak || reqLog)
		{
			char msg[2048];
			FormatMsg(curTotalTimeMs, msg, sizeof(msg));

			if (isPeak)
				s_flashPeakHistory.Add(SFlashPeak(gEnv->pTimer->GetAsyncCurTime(), msg));

			if (reqLog)
				CFlashFunctionProfilerLog::GetAccess().Log(msg);
		}
	}

private:
	void FormatFlashVarValue(const SFlashVarValue* pArg, char* pBuf, size_t sizeBuf) const
	{
		switch (pArg->GetType())
		{
		case SFlashVarValue::eBool:
			cry_sprintf(pBuf, sizeBuf, "%s", pArg->GetBool() ? "true" : "false");
			break;
		case SFlashVarValue::eInt:
			cry_sprintf(pBuf, sizeBuf, "%d", pArg->GetInt());
			break;
		case SFlashVarValue::eUInt:
			cry_sprintf(pBuf, sizeBuf, "%u", pArg->GetUInt());
			break;
		case SFlashVarValue::eDouble:
			cry_sprintf(pBuf, sizeBuf, "%lf", pArg->GetDouble());
			break;
		case SFlashVarValue::eFloat:
			cry_sprintf(pBuf, sizeBuf, "%f", pArg->GetFloat());
			break;
		case SFlashVarValue::eConstStrPtr:
			cry_sprintf(pBuf, sizeBuf, "\"%s\"", pArg->GetConstStrPtr());
			break;
		case SFlashVarValue::eConstWstrPtr:
			cry_sprintf(pBuf, sizeBuf, "\"%ls\"", pArg->GetConstWstrPtr());
			break;
		case SFlashVarValue::eNull:
			cry_sprintf(pBuf, sizeBuf, "null");
			break;
		case SFlashVarValue::eObject:
			cry_sprintf(pBuf, sizeBuf, "obj");
			break;
		case SFlashVarValue::eUndefined:
			cry_sprintf(pBuf, sizeBuf, "???");
			break;
		default:
			assert(0);
			cry_sprintf(pBuf, sizeBuf, "???");
			break;
		}
	}
	void FormatMsg(float funcTimeMs, char* msgBuf, int sizeMsgBuf) const
	{
		char funcArgList[1024];
		cry_strcpy(funcArgList, "");

		for (int i = 0; i < m_numFuncArgs; ++i)
		{
			char curArg[512];
			switch (m_pFuncArgs[i].type)
			{
			case SPODVariant::eBool:
				cry_sprintf(curArg, "%s", m_pFuncArgs[i].data.b ? "true" : "false");
				break;
			case SPODVariant::eInt:
				cry_sprintf(curArg, "%d", m_pFuncArgs[i].data.i);
				break;
			case SPODVariant::eUInt:
				cry_sprintf(curArg, "%u", m_pFuncArgs[i].data.ui);
				break;
			case SPODVariant::eDouble:
				cry_sprintf(curArg, "%lf", m_pFuncArgs[i].data.d);
				break;
			case SPODVariant::eFloat:
				cry_sprintf(curArg, "%f", m_pFuncArgs[i].data.f);
				break;
			case SPODVariant::eConstStrPtr:
				cry_sprintf(curArg, "\"%s\"", m_pFuncArgs[i].data.pStr);
				break;
			case SPODVariant::eConstWstrPtr:
				cry_sprintf(curArg, "\"%ls\"", m_pFuncArgs[i].data.pWstr);
				break;
			case SPODVariant::eConstVoidPtr:
				cry_sprintf(curArg, "0x%p", m_pFuncArgs[i].data.pVoid);
				break;
			case SPODVariant::eFlashVariableArrayType:
				{
					const char* pFlashVarTypeName("FVAT_???");
					switch (m_pFuncArgs[i].data.fvat)
					{
					case FVAT_Int:
						pFlashVarTypeName = "FVAT_Int";
						break;
					case FVAT_Double:
						pFlashVarTypeName = "FVAT_Double";
						break;
					case FVAT_Float:
						pFlashVarTypeName = "FVAT_Float";
						break;
					case FVAT_ConstStrPtr:
						pFlashVarTypeName = "FVAT_ConstStrPtr";
						break;
					case FVAT_ConstWstrPtr:
						pFlashVarTypeName = "FVAT_ConstWstrPtr";
						break;
					default:
						assert(0);
						break;
					}
					cry_sprintf(curArg, "%s", pFlashVarTypeName);
					break;
				}
			case SPODVariant::eFlashVarValuePtr:
				FormatFlashVarValue(m_pFuncArgs[i].data.pfvv, curArg, sizeof(curArg));
				break;
			case SPODVariant::eFlashVarValueList:
				{
					cry_strcpy(curArg, "");
					const SFlashVarValue* const pArgs = m_pFuncArgs[i].data.fvvl.pArgs;
					const unsigned int numArgs = m_pFuncArgs[i].data.fvvl.numArgs;
					for (unsigned int n = 0; n < numArgs; ++n)
					{
						char tmpArg[512];
						FormatFlashVarValue(&pArgs[n], tmpArg, sizeof(tmpArg));

						cry_strcat(curArg, tmpArg);

						if (n < numArgs - 1)
						{
							cry_strcat(curArg, ", ");
						}
					}
					break;
				}
			default:
				cry_strcpy(curArg, "");
				break;
			}

			cry_strcat(funcArgList, curArg);

			if (i < m_numFuncArgs - 1)
			{
				cry_strcat(funcArgList, ", ");
			}
		}

		cry_sprintf(msgBuf, sizeMsgBuf, "%.2f %s(%s) - %s", funcTimeMs, m_pCustomFuncName, funcArgList, m_pFlashFilePath);
	}

public:
	static void ResetPeakFuncExcludeMask()
	{
		ms_peakFuncExcludeMask = 0;
	}
	static void AddToPeakFuncExcludeMask(EFlashFunctionID functionID)
	{
		ms_peakFuncExcludeMask |= 1 << functionID;
	}

private:
	static unsigned int ms_peakFuncExcludeMask;

private:
	uint64              m_funcTime;
	float               m_peakTolerance;
	EFlashFunctionID    m_functionID;
	int                 m_numFuncArgs;
	const SPODVariant*  m_pFuncArgs;
	const char*         m_pCustomFuncName;
	SFlashProfilerData* m_pProfilerData;
	const char*         m_pFlashFilePath;
};

unsigned int CFlashFunctionProfiler::ms_peakFuncExcludeMask(0);

class CFlashFunctionProfilerProxy
{
public:
	CFlashFunctionProfilerProxy(CFlashFunctionProfiler* pProfiler)
		: m_pProfiler(pProfiler)
	{
		if (m_pProfiler)
			m_pProfiler->ProfileEnter();
	}

	~CFlashFunctionProfilerProxy()
	{
		if (m_pProfiler)
			m_pProfiler->ProfilerLeave();
	}

private:
	CFlashFunctionProfiler* m_pProfiler;
};

class CCaptureDrawStats
{
public:
	CCaptureDrawStats(IScaleformRecording* pRenderer, SFlashProfilerData* pProfilerData, bool capture)
		: m_pRenderer(pRenderer)
		, m_pProfilerData(pProfilerData)
		, m_drawCallsPrev(0)
		, m_triCountPrev(0)
		, m_capture(capture)
	{
		assert(m_pRenderer);
		IF (m_capture, 0)
		{
			GRenderer::Stats stats;
			m_pRenderer->GetRenderStats(&stats, false);
			m_drawCallsPrev = stats.Primitives;
			m_triCountPrev = stats.Triangles;
		}
	}

	~CCaptureDrawStats()
	{
		IF (m_capture, 0)
		{
			GRenderer::Stats stats;
			m_pRenderer->GetRenderStats(&stats, false);
			threadID idx = 0;
			gEnv->pRenderer->EF_Query(EFQ_RenderThreadList, idx);
			m_pProfilerData->AddDrawCalls(stats.Primitives - m_drawCallsPrev, idx);
			m_pProfilerData->AddTriCount(stats.Triangles - m_triCountPrev, idx);
		}
	}

private:
	IScaleformRecording* m_pRenderer;
	SFlashProfilerData*  m_pProfilerData;
	uint32               m_drawCallsPrev;
	uint32               m_triCountPrev;
	bool                 m_capture;
};

		#define CAPTURE_DRAW_STATS_BEGIN \
		  {                              \
		    CCaptureDrawStats captureDrawStats(m_pRenderer, m_pProfilerData, ms_sys_flash_info && m_pProfilerData);

		#define CAPTURE_DRAW_STATS_END \
		  }

		#define FREEZE_VAROBJ(defRet)                                      \
		  IF (ms_sys_flash_info && SFlashProfilerData::DisplayFrozen(), 0) \
		    return defRet;

		#define FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)                                                                       \
		  IF ((funcID != eFncDisplay) && !IsFlashEnabled() || ms_sys_flash_info && m_pProfilerData &&                            \
		      (m_pProfilerData->PreventFunctionExectution() || funcID != eFncDisplay && SFlashProfilerData::DisplayFrozen()), 0) \
		    return defRet;

		#define FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(numArgs)                \
		  CFlashFunctionProfiler * pProfiler(0);                               \
		  char memFlashProfiler[sizeof(CFlashFunctionProfiler)];               \
		  char memFuncArgs[(numArgs > 0 ? numArgs : 1) * sizeof(SPODVariant)]; \
		  IF (ms_sys_flash_info, 0)                                            \
		  {                                                                    \
		    int numArgsInit(0);                                                \
		    SPODVariant* pArg((SPODVariant*)memFuncArgs);

		#define FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg) \
		  new(pArg) SPODVariant(arg);                         \
		  ++pArg;                                             \
		  ++numArgsInit;

		#define FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_VALIST(vargList, numArgs) \
		  if (numArgs > 0)                                                     \
		  {                                                                    \
		    SPODVariant::SFlashVarValueList val;                               \
		    val.pArgs = vargList;                                              \
		    val.numArgs = numArgs;                                             \
		    new(pArg) SPODVariant(val);                                        \
		    ++pArg;                                                            \
		    ++numArgsInit;                                                     \
		  }

// Level heap note: the "new SFlashProfilerData" below doesn't go into the GFx mem pool! Used for profiling only and a leak here in fact indicates that the parent flash instance is leaking.
		#define FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)                                                                                               \
		  if (!m_pProfilerData)                                                                                                                                            \
		    /*(SFlashProfilerData*)*/ m_pProfilerData = new SFlashProfilerData;                                                                                            \
		  pProfiler = new(memFlashProfiler) CFlashFunctionProfiler(ms_sys_flash_info_peak_tolerance, funcID, numArgsInit,                                                  \
		                                                           numArgsInit > 0 ? (SPODVariant*)memFuncArgs : 0, funcCustomName, m_pProfilerData, m_filePath->c_str()); \
		  }                                                                                                                                                                \
		  CFlashFunctionProfilerProxy proxy(pProfiler);

		#define VOID_RETURN ((void) 0)

		#define FLASH_PROFILE_FUNC(funcID, defRet, funcCustomName) \
		  FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)               \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(0)                \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

		#define FLASH_PROFILE_FUNC_1ARG(funcID, defRet, funcCustomName, arg0) \
		  FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)                          \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(1)                           \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0)                      \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

		#define FLASH_PROFILE_FUNC_1ARG_VALIST(funcID, defRet, funcCustomName, arg0, vargList, numArgs) \
		  FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)                                                    \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(2)                                                     \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0)                                                \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_VALIST(vargList, numArgs)                                \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

		#define FLASH_PROFILE_FUNC_2ARG(funcID, defRet, funcCustomName, arg0, arg1) \
		  FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)                                \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(2)                                 \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0)                            \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg1)                            \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

		#define FLASH_PROFILE_FUNC_3ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2) \
		  FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)                                      \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(3)                                       \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0)                                  \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg1)                                  \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg2)                                  \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

		#define FLASH_PROFILE_FUNC_4ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2, arg3) \
		  FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)                                            \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(4)                                             \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0)                                        \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg1)                                        \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg2)                                        \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg3)                                        \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

		#define FLASH_PROFILE_FUNC_5ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2, arg3, arg4) \
		  FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)                                                  \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(5)                                                   \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg0)                                              \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg1)                                              \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg2)                                              \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg3)                                              \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg4)                                              \
		  FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

	#else // #if defined(ENABLE_FLASH_INFO)

		#define CAPTURE_DRAW_STATS_BEGIN
		#define CAPTURE_DRAW_STATS_END

		#define FREEZE_VAROBJ(defRet)

		#define FLASH_PROFILE_FUNC_PRECOND(defRet, funcID)
		#define FLASH_PROFILE_FUNC_BUILDPROFILER_BEGIN(numArgs)
		#define FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_ARG(arg)
		#define FLASH_PROFILE_FUNC_BUILDPROFILER_ADD_VALIST(vargList, numArgs)
		#define FLASH_PROFILE_FUNC_BUILDPROFILER_END(funcID, funcCustomName)

		#define VOID_RETURN ((void) 0)

		#define FLASH_PROFILE_FUNC(funcID, defRet, funcCustomName)
		#define FLASH_PROFILE_FUNC_1ARG(funcID, defRet, funcCustomName, arg0)
		#define FLASH_PROFILE_FUNC_1ARG_VALIST(funcID, defRet, funcCustomName, arg0, vargList, numArgs)
		#define FLASH_PROFILE_FUNC_2ARG(funcID, defRet, funcCustomName, arg0, arg1)
		#define FLASH_PROFILE_FUNC_3ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2)
		#define FLASH_PROFILE_FUNC_4ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2, arg3)
		#define FLASH_PROFILE_FUNC_5ARG(funcID, defRet, funcCustomName, arg0, arg1, arg2, arg3, arg4)

	#endif // #if defined(ENABLE_FLASH_INFO)

	#if defined(ENABLE_LW_PROFILERS)

		#define FPL_THREAD_SAFE_COUNTING

class CFlashFunctionProfilerLight
{
public:
	CFlashFunctionProfilerLight()
		: m_startTick(FlashTimer::GetTicks())
	{
		CRY_PROFILE_PUSH_MARKER("Flash");
	}

	~CFlashFunctionProfilerLight()
	{
		uint32 delta = (uint32) (FlashTimer::GetTicks() - m_startTick); // 32 bit should be enough for tick delta

		#if defined(FPL_THREAD_SAFE_COUNTING)
		while (true)
		{
			const uint32 curAccum = ms_deltaTicksAccum;
			IF (curAccum == (uint32) CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&ms_deltaTicksAccum), (LONG) (curAccum + delta), (LONG) curAccum), 1)
				break;
		}
		#else
		ms_deltaTicksAccum += delta;
		#endif
		CRY_PROFILE_POP_MARKER("Flash");
	}

	static void Update()
	{
		ms_hist[ms_histIdx] = FlashTimer::TicksToSeconds(ms_deltaTicksAccum);
		ms_histIdx = (ms_histIdx + 1) & (HIST_SIZE - 1);

		#if defined(FPL_THREAD_SAFE_COUNTING)
		while (true)
		{
			const uint32 curAccum = ms_deltaTicksAccum;
			IF (curAccum == (uint32) CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&ms_deltaTicksAccum), 0, (LONG) curAccum), 1)
				break;
		}
		#else
		ms_deltaTicksAccum = 0;
		#endif
	}

	static float GetAccumulatedTime()
	{
		float tavg = 0.0f;
		for (uint32 i = 0; i < HIST_SIZE; ++i)
			tavg += ms_hist[i];

		return tavg / (float) HIST_SIZE;
	}

private:
	static volatile uint32 ms_deltaTicksAccum;
	static const uint32    HIST_SIZE = 32;
	static uint32          ms_histIdx;
	static float           ms_hist[HIST_SIZE];

private:
	uint64 m_startTick;
};

volatile uint32 CFlashFunctionProfilerLight::ms_deltaTicksAccum = 0;
uint32 CFlashFunctionProfilerLight::ms_histIdx = 0;
float CFlashFunctionProfilerLight::ms_hist[HIST_SIZE] = { 0 };

		#define FLASH_PROFILER_LIGHT CFlashFunctionProfilerLight flashProfileLight;

	#else

		#define FLASH_PROFILER_LIGHT

	#endif // #if defined(ENABLE_LW_PROFILERS)

//////////////////////////////////////////////////////////////////////////
// flash log context

	#if !defined(_RELEASE)
class CFlashLogContext
{
public:
	CFlashLogContext(CryGFxLog* pLog, const char* pFlashContext)
		: m_pLog(pLog)
		, m_pPrevLogContext(0)
	{
		m_pPrevLogContext = pLog->GetContext();
		pLog->SetContext(pFlashContext);
	}

	~CFlashLogContext()
	{
		m_pLog->SetContext(m_pPrevLogContext);
	}

private:
	CryGFxLog*  m_pLog;
	const char* m_pPrevLogContext;
};

		#define SET_LOG_CONTEXT(filePath)      CFlashLogContext logContext(&CryGFxLog::GetAccess(), filePath->c_str());
		#define SET_LOG_CONTEXT_CSTR(filePath) CFlashLogContext logContext(&CryGFxLog::GetAccess(), filePath);

	#else

		#define SET_LOG_CONTEXT(filePath)
		#define SET_LOG_CONTEXT_CSTR(filePath)

	#endif

	#define SYNC_THREADS CryAutoCriticalSection theLock(*m_lock.get());

// Can't fully spinlock Display() calls. When attempted from main thread (CFlashPlayer::RenderRecordLockless())
// it might cause flushing of the render thread cmd buffer (e.g. due to Flash texture creation) onto which other
// Flash assets could have been pushed previously (CFlashPlayer::Render()) causing a dead lock! Use a lock attempt
// for now until we fully switch to lockless Flash rendering.

static CryCriticalSection s_displaySync;

	#define SYNC_DISPLAY_BEGIN        \
	  IF (s_displaySync.TryLock(), 1) \
	  {

	#define SYNC_DISPLAY_END  \
	  s_displaySync.Unlock(); \
	  }

//////////////////////////////////////////////////////////////////////////
// GFxValue <-> SFlashVarValue translation

static inline GFxValue ConvertValue(const SFlashVarValue& src)
{
	//CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	switch (src.GetType())
	{
	case SFlashVarValue::eBool:
		return GFxValue(src.GetBool());

	case SFlashVarValue::eInt:
		return GFxValue((double)src.GetInt());

	case SFlashVarValue::eUInt:
		return GFxValue((double)src.GetUInt());

	case SFlashVarValue::eDouble:
		return GFxValue(src.GetDouble());

	case SFlashVarValue::eFloat:
		return GFxValue((double)src.GetFloat());

	case SFlashVarValue::eConstStrPtr:
		return GFxValue(src.GetConstStrPtr());

	case SFlashVarValue::eConstWstrPtr:
		return GFxValue(src.GetConstWstrPtr());

	case SFlashVarValue::eNull:
		return GFxValue(GFxValue::VT_Null);

	case SFlashVarValue::eObject:
		assert(0);
	case SFlashVarValue::eUndefined:
	default:
		return GFxValue();
	}
}

static inline SFlashVarValue ConvertValue(const GFxValue& src)
{
	//CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	switch (src.GetType())
	{
	case GFxValue::VT_Boolean:
		return SFlashVarValue(src.GetBool());

	case GFxValue::VT_Number:
		return SFlashVarValue(src.GetNumber());

	case GFxValue::VT_String:
		return SFlashVarValue(src.GetString());

	case GFxValue::VT_StringW:
		return SFlashVarValue(src.GetStringW());

	case GFxValue::VT_Null:
		return SFlashVarValue::CreateNull();

	case GFxValue::VT_Object:
	case GFxValue::VT_Array:
	case GFxValue::VT_DisplayObject:
		{
			struct SFlashVarValueObj : public SFlashVarValue
			{
				SFlashVarValueObj() : SFlashVarValue(eObject) {}
			};
			return SFlashVarValueObj();
		}

	case GFxValue::VT_Undefined:
	default:
		return SFlashVarValue::CreateUndefined();
	}
}

static inline bool GetArrayType(EFlashVariableArrayType type, GFxMovie::SetArrayType& translatedType)
{
	//CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	switch (type)
	{
	case FVAT_Int:
		translatedType = GFxMovie::SA_Int;
		return true;
	case FVAT_Double:
		translatedType = GFxMovie::SA_Double;
		return true;
	case FVAT_Float:
		translatedType = GFxMovie::SA_Float;
		return true;
	case FVAT_ConstStrPtr:
		translatedType = GFxMovie::SA_String;
		return true;
	case FVAT_ConstWstrPtr:
		translatedType = GFxMovie::SA_StringW;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// implementation of IFlashVariableObject

class CFlashVariableObject : public IFlashVariableObject
{
public:
	// IFlashVariableObject interface
	virtual void                  Release();
	virtual IFlashVariableObject* Clone() const;

	virtual bool                  IsObject() const;
	virtual bool                  IsArray() const;
	virtual bool                  IsDisplayObject() const;

	virtual SFlashVarValue        ToVarValue() const;

	virtual bool                  HasMember(const char* pMemberName) const;
	virtual bool                  SetMember(const char* pMemberName, const SFlashVarValue& value);
	virtual bool                  SetMember(const char* pMemberName, const IFlashVariableObject* pVarObj);
	virtual bool                  GetMember(const char* pMemberName, SFlashVarValue& value) const;
	virtual bool                  GetMember(const char* pMemberName, IFlashVariableObject*& pVarObj) const;
	virtual void                  VisitMembers(ObjectVisitor* pVisitor) const;
	virtual bool                  DeleteMember(const char* pMemberName);
	virtual bool                  Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0);
	virtual bool                  Invoke(const char* szMethodName, const IFlashVariableObject** pArgs, unsigned int numArgs, SFlashVarValue* pResult = 0);

	virtual unsigned int          GetArraySize() const;
	virtual bool                  SetArraySize(unsigned int size);
	virtual bool                  SetElement(unsigned int idx, const SFlashVarValue& value);
	virtual bool                  SetElement(unsigned int idx, const IFlashVariableObject* pVarObj);
	virtual bool                  GetElement(unsigned int idx, SFlashVarValue& value) const;
	virtual bool                  GetElement(unsigned int idx, IFlashVariableObject*& pVarObj) const;
	virtual bool                  PushBack(const SFlashVarValue& value);
	virtual bool                  PushBack(const IFlashVariableObject* pVarObj);
	virtual bool                  PopBack();
	virtual bool                  RemoveElements(unsigned int idx, int count = -1);

	virtual bool                  SetDisplayInfo(const SFlashDisplayInfo& info);
	virtual bool                  GetDisplayInfo(SFlashDisplayInfo& info) const;
	virtual bool                  SetDisplayMatrix(const Matrix33& mat);
	virtual bool                  GetDisplayMatrix(Matrix33& mat) const;
	virtual bool                  Set3DMatrix(const Matrix44& mat);
	virtual bool                  Get3DMatrix(Matrix44& mat) const;
	virtual bool                  SetColorTransform(const SFlashCxform& cx);
	virtual bool                  GetColorTransform(SFlashCxform& cx) const;

	virtual bool                  SetText(const char* pText);
	virtual bool                  SetText(const wchar_t* pText);
	virtual bool                  SetTextHTML(const char* pHtml);
	virtual bool                  SetTextHTML(const wchar_t* pHtml);
	virtual bool                  GetText(SFlashVarValue& text) const;
	virtual bool                  GetTextHTML(SFlashVarValue& html) const;

	virtual bool                  CreateEmptyMovieClip(IFlashVariableObject*& pVarObjMC, const char* pInstanceName, int depth = -1);
	virtual bool                  AttachMovie(IFlashVariableObject*& pVarObjMC, const char* pSymbolName, const char* pInstanceName, int depth = -1, const IFlashVariableObject* pInitObj = 0);
	virtual bool                  GotoAndPlay(const char* pFrame);
	virtual bool                  GotoAndStop(const char* pFrame);
	virtual bool                  GotoAndPlay(unsigned int frame);
	virtual bool                  GotoAndStop(unsigned int frame);
	virtual bool                  SetVisible(bool visible);

public:
	CFlashVariableObject(const GFxValue& value, const stringConstPtr& refFilePath, const CryCriticalSectionPtr& lock);
	CFlashVariableObject(const CFlashVariableObject& src);
	virtual ~CFlashVariableObject();

	const GFxValue&       GetGFxValue() const;
	const stringConstPtr& GetRefFilePath() const;

public:
	#if defined(USE_GFX_POOL)
	GFC_MEMORY_REDEFINE_NEW(CFlashVariableObject, GStat_Default_Mem)
	void* operator new(size_t, void* p) throw() { return p; }
	void* operator new[](size_t, void* p) throw() { return p; }
	void  operator delete(void*, void*) throw()   {}
	void  operator delete[](void*, void*) throw() {}
	#endif

public:
	typedef FlashHelpers::LinkedResourceList<CFlashVariableObject>           VariableList;
	typedef FlashHelpers::LinkedResourceList<CFlashVariableObject>::NodeType VariableListNodeType;

	static VariableList& GetList()
	{
		return ms_variableList;
	}
	static VariableListNodeType& GetListRoot()
	{
		return ms_variableList.GetRoot();
	}

private:
	static VariableList ms_variableList;

private:
	GFxValue                    m_value;
	const stringConstPtr        m_refFilePath;
	mutable GFxValue            m_retValRefHolder;
	const CryCriticalSectionPtr m_lock;
	VariableListNodeType        m_node;
};

CFlashVariableObject::VariableList CFlashVariableObject::ms_variableList;

static const GFxValue& GetGFxValue(const IFlashVariableObject* p)
{
	assert(p);
	return static_cast<const CFlashVariableObject*>(p)->GetGFxValue();
}

const GFxValue& CFlashVariableObject::GetGFxValue() const
{
	return m_value;
}

const stringConstPtr& CFlashVariableObject::GetRefFilePath() const
{
	return m_refFilePath;
}

CFlashVariableObject::CFlashVariableObject(const GFxValue& value, const stringConstPtr& refFilePath, const CryCriticalSectionPtr& lock)
	: m_value(value)
	, m_refFilePath(refFilePath)
	, m_retValRefHolder()
	, m_lock(lock)
	, m_node()
{
	m_node.m_pHandle = this;
	GetList().Link(m_node);
}

CFlashVariableObject::CFlashVariableObject(const CFlashVariableObject& src)
	: m_value(src.m_value)
	, m_refFilePath(src.m_refFilePath)
	, m_retValRefHolder()
	, m_lock(src.m_lock)
	, m_node()
{
	m_node.m_pHandle = this;
	GetList().Link(m_node);
}

CFlashVariableObject::~CFlashVariableObject()
{
	GetList().Unlink(m_node);
	m_node.m_pHandle = 0;
}

void CFlashVariableObject::Release()
{
	FLASH_PROFILER_LIGHT;

	delete this;
}

IFlashVariableObject* CFlashVariableObject::Clone() const
{
	FLASH_PROFILER_LIGHT;

	return new CFlashVariableObject(*this);
}

bool CFlashVariableObject::IsObject() const
{
	return m_value.IsObject();
}

bool CFlashVariableObject::IsArray() const
{
	return m_value.IsArray();
}

bool CFlashVariableObject::IsDisplayObject() const
{
	return m_value.IsDisplayObject();
}

SFlashVarValue CFlashVariableObject::ToVarValue() const
{
	return ConvertValue(m_value);
}

bool CFlashVariableObject::HasMember(const char* pMemberName) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsObject())
		res = m_value.HasMember(pMemberName);
	return res;
}

bool CFlashVariableObject::SetMember(const char* pMemberName, const SFlashVarValue& value)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsObject())
		res = m_value.SetMember(pMemberName, ConvertValue(value));
	return res;
}

bool CFlashVariableObject::SetMember(const char* pMemberName, const IFlashVariableObject* pVarObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsObject())
		res = m_value.SetMember(pMemberName, pVarObj ? ::GetGFxValue(pVarObj) : GFxValue());
	return res;
}

bool CFlashVariableObject::GetMember(const char* pMemberName, SFlashVarValue& value) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	m_retValRefHolder = GFxValue();
	bool res(false);
	if (m_value.IsObject())
	{
		GFxValue retVal;
		res = m_value.GetMember(pMemberName, &retVal);
		if (res && (retVal.IsString() || retVal.IsStringW()))
			m_retValRefHolder = retVal;
		value = ConvertValue(retVal);
	}
	return res;
}

bool CFlashVariableObject::GetMember(const char* pMemberName, IFlashVariableObject*& pVarObj) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	pVarObj = 0;
	if (m_value.IsObject())
	{
		GFxValue retVal;
		if (m_value.GetMember(pMemberName, &retVal))
		{
			//assert(retVal.IsObject());
			pVarObj = new CFlashVariableObject(retVal, m_refFilePath, m_lock);
		}
	}
	return pVarObj != 0;
}

bool CFlashVariableObject::Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	m_retValRefHolder = GFxValue();
	bool res(false);
	if (m_value.IsObject() && (pArgs || !numArgs))
	{
		assert(!pArgs || numArgs);
		GFxValue* pTranslatedArgs(0);
		if (pArgs && numArgs)
		{
			PREFAST_SUPPRESS_WARNING(6255)
			pTranslatedArgs = (GFxValue*) alloca(numArgs * sizeof(GFxValue));
			if (pTranslatedArgs)
			{
				for (unsigned int i(0); i < numArgs; ++i)
					new(&pTranslatedArgs[i])GFxValue(ConvertValue(pArgs[i]));
			}
		}

		if (pTranslatedArgs || !numArgs)
		{
			GFxValue retVal;
			{
				// This is needed to disable fp exception being triggered in Scaleform
				CScopedFloatingPointException fpExceptionScope(eFPE_None);

				res = m_value.Invoke(pMethodName, &retVal, pTranslatedArgs, numArgs);
			}
			if (pResult)
			{
				if (retVal.IsString() || retVal.IsStringW())
					m_retValRefHolder = retVal;
				*pResult = ConvertValue(retVal);
			}
			for (unsigned int i(0); i < numArgs; ++i)
				pTranslatedArgs[i].~GFxValue();
		}
	}
	return res;
}

bool CFlashVariableObject::Invoke(const char* szMethodName, const IFlashVariableObject** pArgs, unsigned int numArgs, SFlashVarValue* pResult)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	m_retValRefHolder = GFxValue();
	bool res(false);
	if (m_value.IsObject() && pArgs)
	{
		assert(!pArgs || numArgs);
		GFxValue* pTranslatedArgs(0);

		if (pArgs && numArgs)
		{
			PREFAST_SUPPRESS_WARNING(6255)
			pTranslatedArgs = (GFxValue*)alloca(numArgs * sizeof(GFxValue));
			if (pTranslatedArgs)
			{
				for (unsigned int i(0); i < numArgs; ++i)
				{
					GFxValue inputObject = static_cast<const CFlashVariableObject*>(pArgs[i])->GetGFxValue();
					new(&pTranslatedArgs[i])GFxValue(inputObject);
				}
			}
		}

		GFxValue retVal;
		res = m_value.Invoke(szMethodName, &retVal, pTranslatedArgs, numArgs);
		if (pResult)
		{
			if (retVal.IsString() || retVal.IsStringW())
				m_retValRefHolder = retVal;
			*pResult = ConvertValue(retVal);
		}

		for (unsigned int i(0); i < numArgs; ++i)
			pTranslatedArgs[i].~GFxValue();
	}
	return res;
}

void CFlashVariableObject::VisitMembers(ObjectVisitor* pVisitor) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	if (m_value.IsObject() && pVisitor)
	{
		struct VisitorAdaptor : public GFxValue::ObjectVisitor
		{
			IFlashVariableObject::ObjectVisitor* m_pClientVisitor;

			VisitorAdaptor(IFlashVariableObject::ObjectVisitor* pClientVisitor)
				: m_pClientVisitor(pClientVisitor)
			{
			}

			virtual void Visit(const char* name, const GFxValue& val)
			{
				assert(m_pClientVisitor);
				m_pClientVisitor->Visit(name);
			}
		};

		VisitorAdaptor vb(pVisitor);
		m_value.VisitMembers(&vb);
	}
}

bool CFlashVariableObject::DeleteMember(const char* pMemberName)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsObject())
		res = m_value.DeleteMember(pMemberName);
	return res;
}

unsigned int CFlashVariableObject::GetArraySize() const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	unsigned int res(0);
	if (m_value.IsArray())
		res = m_value.GetArraySize();
	return res;
}

bool CFlashVariableObject::SetArraySize(unsigned int size)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsArray())
		res = m_value.SetArraySize(size);
	return res;
}

bool CFlashVariableObject::SetElement(unsigned int idx, const SFlashVarValue& value)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsArray())
		res = m_value.SetElement(idx, ConvertValue(value));
	return res;
}

bool CFlashVariableObject::SetElement(unsigned int idx, const IFlashVariableObject* pVarObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsArray())
		res = m_value.SetElement(idx, pVarObj ? ::GetGFxValue(pVarObj) : GFxValue());
	return res;
}

bool CFlashVariableObject::GetElement(unsigned int idx, SFlashVarValue& value) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	m_retValRefHolder = GFxValue();
	bool res(false);
	if (m_value.IsArray())
	{
		GFxValue retVal;
		res = m_value.GetElement(idx, &retVal);
		if (res && (retVal.IsString() || retVal.IsStringW()))
			m_retValRefHolder = retVal;
		value = ConvertValue(retVal);
	}
	return res;
}

bool CFlashVariableObject::GetElement(unsigned int idx, IFlashVariableObject*& pVarObj) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	pVarObj = 0;
	if (m_value.IsArray())
	{
		GFxValue retVal;
		if (m_value.GetElement(idx, &retVal))
		{
			//assert(retVal.IsObject());
			pVarObj = new CFlashVariableObject(retVal, m_refFilePath, m_lock);
		}
	}
	return pVarObj != 0;
}

bool CFlashVariableObject::PushBack(const SFlashVarValue& value)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsArray())
		res = m_value.PushBack(ConvertValue(value));
	return res;
}

bool CFlashVariableObject::PushBack(const IFlashVariableObject* pVarObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsArray())
		res = m_value.PushBack(pVarObj ? ::GetGFxValue(pVarObj) : GFxValue());
	return res;
}

bool CFlashVariableObject::PopBack()
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsArray())
		res = m_value.PopBack();
	return res;
}

bool CFlashVariableObject::RemoveElements(unsigned int idx, int count)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsArray())
		res = m_value.RemoveElements(idx, count);
	return res;
}

bool CFlashVariableObject::SetDisplayInfo(const SFlashDisplayInfo& info)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		unsigned short varsSet =
		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_X) ? GFxValue::DisplayInfo::V_x : 0) |
		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_Y) ? GFxValue::DisplayInfo::V_y : 0) |
		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_Z) ? GFxValue::DisplayInfo::V_z : 0) |

		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_XScale) ? GFxValue::DisplayInfo::V_xscale : 0) |
		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_YScale) ? GFxValue::DisplayInfo::V_yscale : 0) |
		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_ZScale) ? GFxValue::DisplayInfo::V_zscale : 0) |

		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_Rotation) ? GFxValue::DisplayInfo::V_rotation : 0) |
		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_XRotation) ? GFxValue::DisplayInfo::V_xrotation : 0) |
		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_YRotation) ? GFxValue::DisplayInfo::V_yrotation : 0) |

		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_Alpha) ? GFxValue::DisplayInfo::V_alpha : 0) |
		  (info.IsFlagSet(SFlashDisplayInfo::FDIF_Visible) ? GFxValue::DisplayInfo::V_visible : 0);

		GFxValue::DisplayInfo di;
		di.Initialize(varsSet,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_X) ? info.GetX() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_Y) ? info.GetY() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_Rotation) ? info.GetRotation() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_XScale) ? info.GetXScale() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_YScale) ? info.GetYScale() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_Alpha) ? info.GetAlpha() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_Visible) ? info.GetVisible() : false,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_Z) ? info.GetZ() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_XRotation) ? info.GetXRotation() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_YRotation) ? info.GetYRotation() : 0,
		              info.IsFlagSet(SFlashDisplayInfo::FDIF_ZScale) ? info.GetZScale() : 0);

		res = m_value.SetDisplayInfo(di);
	}
	return res;
}

bool CFlashVariableObject::GetDisplayInfo(SFlashDisplayInfo& info) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GFxValue::DisplayInfo di;
		res = m_value.GetDisplayInfo(&di);
		if (res)
		{
			unsigned short varsSet =
			  (di.IsFlagSet(GFxValue::DisplayInfo::V_x) ? SFlashDisplayInfo::FDIF_X : 0) |
			  (di.IsFlagSet(GFxValue::DisplayInfo::V_y) ? SFlashDisplayInfo::FDIF_Y : 0) |
			  (di.IsFlagSet(GFxValue::DisplayInfo::V_z) ? SFlashDisplayInfo::FDIF_Z : 0) |

			  (di.IsFlagSet(GFxValue::DisplayInfo::V_xscale) ? SFlashDisplayInfo::FDIF_XScale : 0) |
			  (di.IsFlagSet(GFxValue::DisplayInfo::V_yscale) ? SFlashDisplayInfo::FDIF_YScale : 0) |
			  (di.IsFlagSet(GFxValue::DisplayInfo::V_zscale) ? SFlashDisplayInfo::FDIF_ZScale : 0) |

			  (di.IsFlagSet(GFxValue::DisplayInfo::V_rotation) ? SFlashDisplayInfo::FDIF_Rotation : 0) |
			  (di.IsFlagSet(GFxValue::DisplayInfo::V_xrotation) ? SFlashDisplayInfo::FDIF_XRotation : 0) |
			  (di.IsFlagSet(GFxValue::DisplayInfo::V_yrotation) ? SFlashDisplayInfo::FDIF_YRotation : 0) |

			  (di.IsFlagSet(GFxValue::DisplayInfo::V_alpha) ? SFlashDisplayInfo::FDIF_Alpha : 0) |
			  (di.IsFlagSet(GFxValue::DisplayInfo::V_visible) ? SFlashDisplayInfo::FDIF_Visible : 0);

			info = SFlashDisplayInfo(
			  di.IsFlagSet(GFxValue::DisplayInfo::V_x) ? (float) di.GetX() : 0,
			  di.IsFlagSet(GFxValue::DisplayInfo::V_y) ? (float) di.GetY() : 0,
			  di.IsFlagSet(GFxValue::DisplayInfo::V_z) ? (float) di.GetZ() : 0,

			  di.IsFlagSet(GFxValue::DisplayInfo::V_xscale) ? (float) di.GetXScale() : 0,
			  di.IsFlagSet(GFxValue::DisplayInfo::V_yscale) ? (float) di.GetYScale() : 0,
			  di.IsFlagSet(GFxValue::DisplayInfo::V_zscale) ? (float) di.GetZScale() : 0,

			  di.IsFlagSet(GFxValue::DisplayInfo::V_rotation) ? (float) di.GetRotation() : 0,
			  di.IsFlagSet(GFxValue::DisplayInfo::V_xrotation) ? (float) di.GetXRotation() : 0,
			  di.IsFlagSet(GFxValue::DisplayInfo::V_yrotation) ? (float) di.GetYRotation() : 0,

			  di.IsFlagSet(GFxValue::DisplayInfo::V_alpha) ? (float) di.GetAlpha() : 0,
			  di.IsFlagSet(GFxValue::DisplayInfo::V_visible) ? di.GetVisible() : false,
			  varsSet);
		}
	}
	return res;
}

bool CFlashVariableObject::SetVisible(bool visible)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GFxValue::DisplayInfo di;
		di.Initialize(GFxValue::DisplayInfo::V_visible, 0, 0, 0, 0, 0, 0, visible, 0, 0, 0, 0);
		//di.SetVisible(visible);
		res = m_value.SetDisplayInfo(di);
	}
	return res;
}

bool CFlashVariableObject::SetDisplayMatrix(const Matrix33& mat)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GMatrix2D m(mat.m00, mat.m01, mat.m10, mat.m11, mat.m02, mat.m12);
		res = m_value.SetDisplayMatrix(m);
	}
	return res;
}

bool CFlashVariableObject::GetDisplayMatrix(Matrix33& mat) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GMatrix2D m;
		res = m_value.GetDisplayMatrix(&m);
		if (res)
		{
			mat.m00 = m.M_[0][0];
			mat.m01 = m.M_[0][1];
			mat.m02 = m.M_[0][2];
			mat.m10 = m.M_[1][0];
			mat.m11 = m.M_[1][1];
			mat.m12 = m.M_[1][2];
			mat.m20 = 0;
			mat.m21 = 0;
			mat.m22 = 1;
		}
	}
	return res;
}

bool CFlashVariableObject::Set3DMatrix(const Matrix44& mat)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		assert(fabsf(mat.Determinant()) > 1e-4f);
		GMatrix3D m;
		m.M_[0][0] = mat.m00;
		m.M_[0][1] = mat.m10;
		m.M_[0][2] = mat.m20;
		m.M_[0][3] = mat.m30;
		m.M_[1][0] = mat.m01;
		m.M_[1][1] = mat.m11;
		m.M_[1][2] = mat.m21;
		m.M_[1][3] = mat.m31;
		m.M_[2][0] = mat.m02;
		m.M_[2][1] = mat.m12;
		m.M_[2][2] = mat.m22;
		m.M_[2][3] = mat.m32;
		m.M_[3][0] = mat.m03;
		m.M_[3][1] = mat.m13;
		m.M_[3][2] = mat.m23;
		m.M_[3][3] = mat.m33;
		res = m_value.SetMatrix3D(m);
	}
	return res;
}

bool CFlashVariableObject::Get3DMatrix(Matrix44& mat) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GMatrix3D m;
		res = m_value.GetMatrix3D(&m);
		if (res)
		{
			mat.m00 = m.M_[0][0];
			mat.m01 = m.M_[1][0];
			mat.m02 = m.M_[2][0];
			mat.m03 = m.M_[3][0];
			mat.m10 = m.M_[0][1];
			mat.m11 = m.M_[1][1];
			mat.m12 = m.M_[2][1];
			mat.m13 = m.M_[3][1];
			mat.m20 = m.M_[0][2];
			mat.m21 = m.M_[1][2];
			mat.m22 = m.M_[2][2];
			mat.m23 = m.M_[3][2];
			mat.m30 = m.M_[0][3];
			mat.m31 = m.M_[1][3];
			mat.m32 = m.M_[2][3];
			mat.m33 = m.M_[3][3];
		}
	}
	return res;
}

bool CFlashVariableObject::SetColorTransform(const SFlashCxform& cx)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GRenderer::Cxform cxform;
		cxform.M_[0][0] = cx.mul.r;
		cxform.M_[0][1] = static_cast<float>(cx.add.r);
		cxform.M_[1][0] = cx.mul.g;
		cxform.M_[1][1] = static_cast<float>(cx.add.g);
		cxform.M_[2][0] = cx.mul.b;
		cxform.M_[2][1] = static_cast<float>(cx.add.b);
		cxform.M_[3][0] = cx.mul.a;
		cxform.M_[3][1] = static_cast<float>(cx.add.a);
		res = m_value.SetColorTransform(cxform);
	}
	return res;
}

bool CFlashVariableObject::GetColorTransform(SFlashCxform& cx) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GRenderer::Cxform cxform;
		res = m_value.GetColorTransform(&cxform);
		if (res)
		{
			cx.mul.r = cxform.M_[0][0];
			cx.add.r = static_cast<uint8>(cxform.M_[0][1]);
			cx.mul.g = cxform.M_[1][0];
			cx.add.g = static_cast<uint8>(cxform.M_[1][1]);
			cx.mul.b = cxform.M_[2][0];
			cx.add.b = static_cast<uint8>(cxform.M_[2][1]);
			cx.mul.a = cxform.M_[3][0];
			cx.add.a = static_cast<uint8>(cxform.M_[3][1]);
		}
	}
	return res;
}

bool CFlashVariableObject::SetText(const char* pText)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
		res = m_value.SetText(pText);
	return res;
}

bool CFlashVariableObject::SetText(const wchar_t* pText)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
		res = m_value.SetText(pText);
	return res;
}

bool CFlashVariableObject::SetTextHTML(const char* pHtml)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
		res = m_value.SetTextHTML(pHtml);
	return res;
}

bool CFlashVariableObject::SetTextHTML(const wchar_t* pHtml)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
		res = m_value.SetTextHTML(pHtml);
	return res;
}

bool CFlashVariableObject::GetText(SFlashVarValue& text) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	m_retValRefHolder = GFxValue();
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GFxValue retVal;
		res = m_value.GetText(&retVal);
		if (res && (retVal.IsString() || retVal.IsStringW()))
			m_retValRefHolder = retVal;
		text = ConvertValue(retVal);
	}
	return res;
}

bool CFlashVariableObject::GetTextHTML(SFlashVarValue& html) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	m_retValRefHolder = GFxValue();
	bool res(false);
	if (m_value.IsDisplayObject())
	{
		GFxValue retVal;
		res = m_value.GetTextHTML(&retVal);
		if (res && (retVal.IsString() || retVal.IsStringW()))
			m_retValRefHolder = retVal;
		html = ConvertValue(retVal);
	}
	return res;
}

bool CFlashVariableObject::CreateEmptyMovieClip(IFlashVariableObject*& pVarObjMC, const char* pInstanceName, int depth)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	pVarObjMC = 0;
	if (m_value.IsDisplayObject())
	{
		GFxValue retVal;
		if (m_value.CreateEmptyMovieClip(&retVal, pInstanceName, depth))
		{
			//assert(retVal.IsObject());
			pVarObjMC = new CFlashVariableObject(retVal, m_refFilePath, m_lock);
		}
	}
	return pVarObjMC != 0;
}

bool CFlashVariableObject::AttachMovie(IFlashVariableObject*& pVarObjMC, const char* pSymbolName, const char* pInstanceName, int depth, const IFlashVariableObject* pInitObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	SET_LOG_CONTEXT(m_refFilePath);
	pVarObjMC = 0;
	if (m_value.IsDisplayObject())
	{
		GFxValue retVal;
		if (m_value.AttachMovie(&retVal, pSymbolName, pInstanceName, depth, pInitObj ? &::GetGFxValue(pInitObj) : 0))
		{
			//assert(retVal.IsObject());
			pVarObjMC = new CFlashVariableObject(retVal, m_refFilePath, m_lock);
		}
	}
	return pVarObjMC != 0;
}

bool CFlashVariableObject::GotoAndPlay(const char* pFrame)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
		res = m_value.GotoAndPlay(pFrame);
	return res;
}

bool CFlashVariableObject::GotoAndStop(const char* pFrame)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
		res = m_value.GotoAndStop(pFrame);
	return res;
}

bool CFlashVariableObject::GotoAndPlay(unsigned int frame)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
		res = m_value.GotoAndPlay(frame);
	return res;
}

bool CFlashVariableObject::GotoAndStop(unsigned int frame)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	FREEZE_VAROBJ(true);

	SET_LOG_CONTEXT(m_refFilePath);
	bool res(false);
	if (m_value.IsDisplayObject())
		res = m_value.GotoAndStop(frame);
	return res;
}

//////////////////////////////////////////////////////////////////////////
// implementation of IFlashPlayerBootStrapper

class CFlashPlayerBootStrapper : public IFlashPlayerBootStrapper
{
public:
	// IFlashPlayerBootStrapper interface
	virtual void          Release();
	virtual bool          Load(const char* pFilePath);
	virtual const char*   GetFilePath() const;
	virtual size_t        GetMetadata(char* pBuff, unsigned int buffSize) const;
	virtual bool          HasMetadata(const char* pTag) const;

	virtual std::shared_ptr<IFlashPlayer> CreatePlayerInstance(unsigned int options = IFlashPlayer::DEFAULT, unsigned int cat = IFlashPlayer::eCat_Default);

public:
	CFlashPlayerBootStrapper();
	virtual ~CFlashPlayerBootStrapper();

public:
	#if defined(USE_GFX_POOL)
	GFC_MEMORY_REDEFINE_NEW(CFlashPlayerBootStrapper, GStat_Default_Mem)
	void* operator new(size_t, void* p) throw() { return p; }
	void* operator new[](size_t, void* p) throw() { return p; }
	void  operator delete(void*, void*) throw()   {}
	void  operator delete[](void*, void*) throw() {}
	#endif

public:
	const GFxMovieDef* GetMovieDef() const;

public:
	typedef FlashHelpers::LinkedResourceList<CFlashPlayerBootStrapper>           BootStrapperList;
	typedef FlashHelpers::LinkedResourceList<CFlashPlayerBootStrapper>::NodeType BootStrapperListNodeType;

	static BootStrapperList& GetList()
	{
		return ms_bootstrapperList;
	}
	static BootStrapperListNodeType& GetListRoot()
	{
		return ms_bootstrapperList.GetRoot();
	}

private:
	static BootStrapperList ms_bootstrapperList;

private:
	GPtr<GFxMovieDef>        m_pMovieDef;
	GPtr<GFxLoader2>         m_pLoader;
	BootStrapperListNodeType m_node;
};

CFlashPlayerBootStrapper::BootStrapperList CFlashPlayerBootStrapper::ms_bootstrapperList;

CFlashPlayerBootStrapper::CFlashPlayerBootStrapper()
	: m_pMovieDef(0)
	, m_pLoader(0)
{
	m_pLoader = *CSharedFlashPlayerResources::GetAccess().GetLoader();
	assert(m_pLoader.GetPtr() != 0);

	m_node.m_pHandle = this;
	GetList().Link(m_node);
}

CFlashPlayerBootStrapper::~CFlashPlayerBootStrapper()
{
	GetList().Unlink(m_node);
	m_node.m_pHandle = 0;

	m_pMovieDef = 0;
	m_pLoader = 0;
}

void CFlashPlayerBootStrapper::Release()
{
	delete this;
}

const GFxMovieDef* CFlashPlayerBootStrapper::GetMovieDef() const
{
	return m_pMovieDef;
}

bool CFlashPlayerBootStrapper::Load(const char* pFilePath)
{
	SET_LOG_CONTEXT_CSTR(pFilePath);

	m_pMovieDef = *m_pLoader->CreateMovie(pFilePath, GFxLoader::LoadAll);
	return m_pMovieDef.GetPtr() != 0;
}

std::shared_ptr<IFlashPlayer> CFlashPlayerBootStrapper::CreatePlayerInstance(unsigned int options, unsigned int cat)
{
	return m_pMovieDef.GetPtr() != 0 ? CFlashPlayer::CreateBootstrapped(m_pMovieDef, options, cat) : 0;
}

const char* CFlashPlayerBootStrapper::GetFilePath() const
{
	return m_pMovieDef.GetPtr() != 0 ? m_pMovieDef->GetFileURL() : 0;
}

size_t CFlashPlayerBootStrapper::GetMetadata(char* pBuff, unsigned int buffSize) const
{
	return m_pMovieDef.GetPtr() != 0 ? m_pMovieDef->GetMetadata(pBuff, buffSize) : 0;
}

bool CFlashPlayerBootStrapper::HasMetadata(const char* pTag) const
{
	bool bResult = false;
	const size_t buffSize = GetMetadata(0, 0);
	if (buffSize)
	{
		char* pMetaData = new char[buffSize];
		if (pMetaData)
		{
			GetMetadata(pMetaData, buffSize);
			bResult = CryStringUtils::stristr(pMetaData, pTag) != 0;
		}
		delete[] pMetaData;
	}

	return bResult;
}

//////////////////////////////////////////////////////////////////////////
// implementation of IFlashPlayer

	#if defined(ENABLE_FLASH_INFO)
ICVar* CFlashPlayer::CV_sys_flash_info_peak_exclude(0);
	#endif

int CFlashPlayer::ms_sys_flash(1);
int CFlashPlayer::ms_sys_flash_edgeaa(1);
	#if defined(ENABLE_FLASH_INFO)
//int CFlashPlayer::ms_sys_flash_info(0);
float CFlashPlayer::ms_sys_flash_info_peak_tolerance(5.0f);
float CFlashPlayer::ms_sys_flash_info_histo_scale(1.0f);
	#endif
int CFlashPlayer::ms_sys_flash_log_options(0);
float CFlashPlayer::ms_sys_flash_curve_tess_error(1.0f);
int CFlashPlayer::ms_sys_flash_warning_level(1);
int CFlashPlayer::ms_sys_flash_static_pool_size(0);
int CFlashPlayer::ms_sys_flash_address_space_kb(DEFAULT_VA_SPACE_IN_KB);
int CFlashPlayer::ms_sys_flash_allow_mesh_cache_reset(1);
int CFlashPlayer::ms_sys_flash_reset_mesh_cache(0);
int CFlashPlayer::ms_sys_flash_check_filemodtime(0);
int CFlashPlayer::ms_sys_flash_mipmaps(0);

CFlashPlayer::PlayerList CFlashPlayer::ms_playerList;
IFlashLoadMovieHandler* CFlashPlayer::ms_pLoadMovieHandler(0);

CFlashPlayer::CFlashPlayer()
	: m_clearFlags(0)
	, m_clearColor(Clr_Transparent)
	, m_compDepth(0)
	, m_stereoCustomMaxParallax(-1.0f)
	, m_allowEgdeAA(false)
	, m_stereoFixedProjDepth(false)
	, m_avoidStencilClear(false)
	, m_maskedRendering(false)
	, m_memArenaID(0)
	, m_extendCanvasToVP(false)
	, m_renderConfig()
	, m_asVerbosity()
	, m_frameCount(0)
	, m_frameRate(0)
	, m_width(0)
	, m_height(0)
	, m_pFSCmdHandler(0)
	, m_pFSCmdHandlerUserData(0)
	, m_pEIHandler(0)
	, m_pEIHandlerUserData(0)
	, m_pMovieDef(0)
	, m_pMovieView(0)
	, m_pLoader(0)
	, m_pRenderer(0)
	, m_filePath(new string) // Level heap note: this new doesn't go into the GFx mem pool! Then again the counter object of the shared ptr doesn't neither.
	, m_node()
	#if defined(ENABLE_FLASH_INFO)
	, m_pProfilerData(0)
	#endif
	, m_lock(new CryCriticalSection) // Level heap note: this new doesn't go into the GFx mem pool! Then again the counter object of the shared ptr doesn't neither.
	, m_retValRefHolder()
	, m_cmdBuf()
	#if defined(ENABLE_DYNTEXSRC_PROFILING)
	, m_pDynTexSrc(0)
	#endif
{
	assert(m_lock.get());
	assert(m_filePath.get());

	m_pLoader = *CSharedFlashPlayerResources::GetAccess().GetLoader();
	m_pRenderer = *CSharedFlashPlayerResources::GetAccess().GetRenderer();

	//m_cmdBuf.Init(GMemory::GetGlobalHeap());

	m_node.m_pHandle = this;
	GetList().Link(m_node);
}

CFlashPlayer::~CFlashPlayer()
{
	CSharedFlashPlayerResources::GetAccess().SetImeFocus(m_pMovieView, false);

	GetList().Unlink(m_node);
	m_node.m_pHandle = 0;

	// release ref counts of all shared resources
	{
		SET_LOG_CONTEXT(m_filePath);

		m_cmdBuf.Release();

		m_retValRefHolder = GFxValue();

	#if !defined(_RELEASE)
		const long extRefs = m_filePath.use_count() - 1;
		if (extRefs > 0)
		{
			CryGFxLog::GetAccess().LogError("Releasing flash player object while still holding %d associated flash variable object reference(s)! Enforce breaking into the debugger...", (int) extRefs);
			//__debugbreak();
		}
	#endif

		m_pMovieView = 0;
		m_pMovieDef = 0;

		m_pLoader = 0;
		m_pRenderer = 0;
	}

	CSharedFlashPlayerResources::GetAccess().DestoryMemoryArena(m_memArenaID);

	#if defined(ENABLE_FLASH_INFO)
	SAFE_DELETE(m_pProfilerData);
	#endif
}

static void OnSysFlashInfoLogOptionsChanged(ICVar*)
{
	GFxLoader2* pLoader(CSharedFlashPlayerResources::GetAccess().GetLoader(true));
	if (pLoader)
		pLoader->UpdateParserVerbosity();
}

void CFlashPlayer::InitCVars()
{
	#if defined(_DEBUG)
	{
		static bool s_init = false;
		assert(!s_init);
		s_init = true;
	}
	#endif

	REGISTER_CVAR2("sys_flash", &ms_sys_flash, 1,
	               VF_CHEAT | VF_CHEAT_NOCHECK, "Enables/disables execution of flash files.");

	REGISTER_CVAR2("sys_flash_edgeaa", &ms_sys_flash_edgeaa, 1,
	               0, "Enables/disables edge anti-aliased rendering of flash files.");

	#if defined(ENABLE_FLASH_INFO)
		#if defined(USE_GFX_VIDEO)
			#define GFX_VIDEO_CVAR_INFO "\nDisplay video heap details (5)"
		#else
			#define GFX_VIDEO_CVAR_INFO
		#endif

		#if defined(ENABLE_DYNTEXSRC_PROFILING)
			#define DYNTEXSRC_CVAR_INFO "\nShow dyn tex src related info (6)"
		#else
			#define DYNTEXSRC_CVAR_INFO
		#endif

	REGISTER_CVAR2("sys_flash_info", &ms_sys_flash_info, 0,
	               0, "Enable flash profiling (1)\nDisplay draw statistics (2)\nDisplay font cache texture (3)\nDisplay full heap info (4)" GFX_VIDEO_CVAR_INFO "" DYNTEXSRC_CVAR_INFO);

		#undef GFX_VIDEO_CVAR_INFO

	REGISTER_CVAR2("sys_flash_info_peak_tolerance", &ms_sys_flash_info_peak_tolerance, 5.0f,
	               0, "Defines tolerance value for peaks (in ms) inside the flash profiler.");

	CV_sys_flash_info_peak_exclude = REGISTER_STRING("sys_flash_info_peak_exclude", "",
	                                                 0, "Comma separated list of flash functions to excluded from peak history.");

	REGISTER_CVAR2("sys_flash_info_histo_scale", &ms_sys_flash_info_histo_scale, 1.0f,
	               0, "Defines scaling of function histogram inside the flash profiler.");
	#endif

	REGISTER_CVAR2_CB("sys_flash_log_options", &ms_sys_flash_log_options, 0,
	                  0, "Enables logging of several flash related aspects (add them to combine logging)...\n"
	                     "1) Flash loading                                                       : 1\n"
	                     "2) Flash actions script execution                                      : 2\n"
	                     "3) Flash related high-level calls inspected by the profiler (flash.log): 4\n"
	                     "   Please note that for (3) the following cvars apply:\n"
	                     "   * sys_flash_info\n"
	                     "   * sys_flash_info_peak_exclude",
	                  OnSysFlashInfoLogOptionsChanged);

	REGISTER_CVAR2("sys_flash_curve_tess_error", &ms_sys_flash_curve_tess_error, 1.0f,
	               0, "Controls curve tessellation. Larger values result in coarser, more angular curves.");

	#if defined(_RELEASE) && !CRY_PLATFORM_DESKTOP
	ms_sys_flash_warning_level = 0;
	#else
	REGISTER_CVAR2("sys_flash_warning_level", &ms_sys_flash_warning_level, 1,
	               0, "Sets verbosity level for CryEngine related warnings...\n"
	                  "0) Omit warning\n"
	                  "1) Log warning\n"
	                  "2) Log warning and display message box");
	#endif

	REGISTER_CVAR2("sys_flash_static_pool_size", &ms_sys_flash_static_pool_size, 0, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in kb) of the static memory pool for flash objects.\n"
	               "Set to zero to turn it off and use a dynamic pool instead.");

	REGISTER_CVAR2("sys_flash_address_space", &ms_sys_flash_address_space_kb, DEFAULT_VA_SPACE_IN_KB, VF_REQUIRE_APP_RESTART,
	               "Specifies the size (in kilo bytes) of the address space used for flash objects.\n");

	REGISTER_CVAR2("sys_flash_allow_reset_mesh_cache", &ms_sys_flash_allow_mesh_cache_reset, 1, 0,
	               "Allow programmatic mesh cache resets.");

	REGISTER_CVAR2("sys_flash_reset_mesh_cache", &ms_sys_flash_reset_mesh_cache, 0, 0,
	               "Reset mesh cache through console (once).");

	#if !defined(_RELEASE) || CRY_PLATFORM_WINDOWS || CRY_PLATFORM_APPLE || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID
	REGISTER_CVAR2("sys_flash_check_filemodtime", &ms_sys_flash_check_filemodtime, ms_sys_flash_check_filemodtime, 0,
	               "Respect file modification time for Flash internal resource sharing.\n"
	               "Cached resources with same filepath but different file modification time are treated as unique entities.");
	#endif

	REGISTER_CVAR2("sys_flash_mipmaps", &ms_sys_flash_mipmaps, 0, 0, "Enables/disables mipmap support for flash ui icons");

	//	CScaleformRecording::InitCVars();
	#if defined(USE_GFX_VIDEO)
	GFxVideoWrapper::InitCVars();
	#endif
	GFxVideoCrySoundSystem::InitCVars();
	GFxMemoryArenaWrapper::InitCVars();
}

int CFlashPlayer::GetWarningLevel()
{
	return ms_sys_flash_warning_level;
}

bool CFlashPlayer::CheckFileModTimeEnabled()
{
	return ms_sys_flash_check_filemodtime != 0;
}

bool CFlashPlayer::ConstructInternal(const char* pFilePath, GFxMovieDef* pMovieDef, unsigned int options, unsigned int cat)
{
	if (!pMovieDef || !pFilePath)
		return false;

	m_memArenaID = CSharedFlashPlayerResources::GetAccess().CreateMemoryArena(cat & ~eCat_RequestMeshCacheResetBit, (cat & eCat_RequestMeshCacheResetBit) != 0);

	// create movie view
	GPtr<GFxMovieView> pMovieView = *pMovieDef->CreateInstance((options & INIT_FIRST_FRAME) != 0, m_memArenaID);
	if (!pMovieView)
		return false;

	// create shared string of file path
	*m_filePath = pFilePath;

	// setup render command buffer
	//m_cmdBuf.Release();
	m_cmdBuf.Init(pMovieView->GetHeap());

	// set movie definition and movie view
	m_pMovieDef = pMovieDef;
	m_pMovieView = pMovieView;

	// set action script verbosity
	UpdateASVerbosity();
	m_pMovieView->SetActionControl(&m_asVerbosity);

	// register custom renderer and set renderer flags
	unsigned int renderFlags(m_renderConfig.GetRenderFlags());
	renderFlags &= ~GFxRenderConfig::RF_StrokeMask;
	renderFlags |= GFxRenderConfig::RF_StrokeNormal;
	m_allowEgdeAA = (options & RENDER_EDGE_AA) != 0;
	if (m_allowEgdeAA)
		renderFlags |= GFxRenderConfig::RF_EdgeAA;
	m_renderConfig.SetRenderFlags(renderFlags);
	m_renderConfig.SetRenderer(m_pRenderer);
	m_pMovieView->SetRenderConfig(&m_renderConfig);

	// Reset viewport here so we set a correct viewport after the first update
	// if the movie uses the same resolution as the game. CreateInstance above
	// relies on a properly set GRenderer which we set later, resulting in
	// view and perspective matrices not set.
	m_pMovieView->SetViewport(1, 1, 1, 1, 1, 1);

	// register action script (fs & external interface) and user event command handler,
	// please note that client has to implement IFSCommandHandler interface
	// and set it via IFlashPlayer::SetFSCommandHandler() to provide hook
	m_pMovieView->SetUserData(this);
	m_pMovieView->SetFSCommandHandler(&CryGFxFSCommandHandler::GetAccess());
	m_pMovieView->SetExternalInterface(&CryGFxExternalInterface::GetAccess());
	m_pMovieView->SetUserEventHandler(&CryGFxUserEventHandler::GetAccess());

	// enable mouse support
	m_pMovieView->SetMouseCursorCount((options & ENABLE_MOUSE_SUPPORT) ? 1 : 0);

	// cache constant data for fast lock free lookup
	*const_cast<unsigned int*>(&m_frameCount) = m_pMovieView->GetFrameCount();
	*const_cast<float*>(&m_frameRate) = m_pMovieView->GetFrameRate();
	*const_cast<int*>(&m_width) = (int) m_pMovieDef->GetWidth();
	*const_cast<int*>(&m_height) = (int) m_pMovieDef->GetHeight();

	return true;
}
bool CFlashPlayer::Load(const char* pFilePath, unsigned int options, unsigned int cat)
{
	if (!pFilePath || !pFilePath[0])
		return false;

	FLASH_PROFILER_LIGHT;

	//LOADING_TIME_PROFILE_SECTION;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "FlashPlayer::Load(%s)", pFilePath);

	SET_LOG_CONTEXT_CSTR(pFilePath);

#if defined(USE_GFX_VIDEO)
	string videoPlayerArgString;
	int vidWidth = 0;
	int vidHeight = 0;
	const bool isVideoFile = IsVideoFile(pFilePath);
	if (!isVideoFile)
	{
#endif
		const int warningLevel = GetWarningLevel();
		if (warningLevel)
		{
			GFxMovieInfo movieInfo;
			if (!m_pLoader->GetMovieInfo(pFilePath, &movieInfo))
				return false;

			if (!movieInfo.IsStripped())
			{
				switch (warningLevel)
				{
				case 1:
					CryGFxLog::GetAccess().LogWarning("Trying to load non-stripped flash movie! Use gfxexport to strip movies and generate optimized assets.");
					break;
				case 2:
					CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "!Trying to load non-stripped flash movie! "
						"Use gfxexport to strip movies and generate optimized assets. [%s]", pFilePath);
					break;
				default:
					break;
				}
			}
		}
#if defined(USE_GFX_VIDEO)
	}
	else
	{
		GFxVideoPlayer::VideoInfo videoInfo;
		if (GFxVideoPlayer::LoadVideoInfo(pFilePath, &videoInfo, m_pLoader->GetFileOpener()))
		{
			vidWidth = videoInfo.Width;
			vidHeight = videoInfo.Height;
			if (IsStereoVideoFile(pFilePath))
				vidWidth = (vidWidth << 1) / 3;
		}
		else
		{
			CryGFxLog::GetAccess().LogError("Error: GFxLoader failed to open '%s'", pFilePath);
			return false;
		}

		videoPlayerArgString = pFilePath;
		pFilePath = internal_video_player;
	}
#endif

	// create movie
	GPtr<GFxMovieDef> pMovieDef = *m_pLoader->CreateMovie(pFilePath, GFxLoader::LoadAll);
	if (!pMovieDef)
		return false;

#if defined(USE_GFX_VIDEO)
	options = isVideoFile ? (options & ~INIT_FIRST_FRAME) : options;
#endif
	if (!ConstructInternal(pMovieDef->GetFileURL(), pMovieDef, options, cat))
		return false;

#if defined(USE_GFX_VIDEO)
	if (isVideoFile)
	{
		*const_cast<int*>(&m_width) = vidWidth;
		*const_cast<int*>(&m_height) = vidHeight;
		if (!videoPlayerArgString.empty())
			m_pMovieView->SetVariable("_global.gfxArg", videoPlayerArgString.c_str());
	}
#endif

	// done
	return true;
}

bool CFlashPlayer::Bootstrap(GFxMovieDef* pMovieDef, unsigned int options, unsigned int cat)
{
	if (!pMovieDef)
		return false;

	FLASH_PROFILER_LIGHT;

	//LOADING_TIME_PROFILE_SECTION;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	const char* pFilePath = pMovieDef->GetFileURL();

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "FlashPlayer::Bootstrap(%s)", pFilePath);

	SET_LOG_CONTEXT_CSTR(pFilePath);

	return ConstructInternal(pFilePath, pMovieDef, options, cat);
}

std::shared_ptr<CFlashPlayer> CFlashPlayer::CreateBootstrapped(GFxMovieDef* pMovieDef, unsigned int options, unsigned int cat)
{
	std::shared_ptr<CFlashPlayer> pPlayer;
	if (pMovieDef)
	{
		pPlayer = std::make_shared<CFlashPlayer>();
		if (pPlayer && !pPlayer->Bootstrap(pMovieDef, options, cat))
			pPlayer = nullptr;
	}
	return pPlayer;
}

void CFlashPlayer::SetBackgroundColor(const ColorB& color)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GColor col(color.r, color.g, color.b, color.a);
		m_pMovieView->SetBackgroundColor(col);
	}
}

void CFlashPlayer::SetBackgroundAlpha(float alpha)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		m_pMovieView->SetBackgroundAlpha(alpha);
	}
}

float CFlashPlayer::GetBackgroundAlpha() const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	float res = 1.0f;
	if (m_pMovieView)
	{
		res = m_pMovieView->GetBackgroundAlpha();
	}
	return res;
}

void CFlashPlayer::SetViewport(int x0, int y0, int width, int height, float aspectRatio)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GViewport viewport(width, height, x0, y0, width, height, 0); // invalidates scissor rect!
		viewport.AspectRatio = aspectRatio > 0 ? aspectRatio : 1;
		m_pMovieView->SetViewport(viewport);
	}
}

void CFlashPlayer::SetViewScaleMode(EScaleModeType scaleMode)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GFxMovieView::ScaleModeType sm;
		switch (scaleMode)
		{
		case eSM_NoScale:
			sm = GFxMovieView::SM_NoScale;
			break;
		case eSM_ShowAll:
			sm = GFxMovieView::SM_ShowAll;
			break;
		case eSM_ExactFit:
			sm = GFxMovieView::SM_ExactFit;
			break;
		case eSM_NoBorder:
			sm = GFxMovieView::SM_NoBorder;
			break;
		default:
			assert(0);
			sm = GFxMovieView::SM_NoScale;
			break;
		}
		m_pMovieView->SetViewScaleMode(sm);
	}
}

IFlashPlayer::EScaleModeType CFlashPlayer::GetViewScaleMode() const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	EScaleModeType scaleMode = eSM_NoScale;
	if (m_pMovieView)
	{
		GFxMovieView::ScaleModeType sm = m_pMovieView->GetViewScaleMode();
		switch (sm)
		{
		case GFxMovieView::SM_NoScale:
			scaleMode = eSM_NoScale;
			break;
		case GFxMovieView::SM_ShowAll:
			scaleMode = eSM_ShowAll;
			break;
		case GFxMovieView::SM_ExactFit:
			scaleMode = eSM_ExactFit;
			break;
		case GFxMovieView::SM_NoBorder:
			scaleMode = eSM_NoBorder;
			break;
		default:
			assert(0);
			scaleMode = eSM_NoScale;
			break;
		}
	}
	return scaleMode;
}

void CFlashPlayer::SetViewAlignment(EAlignType viewAlignment)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GFxMovieView::AlignType va;
		switch (viewAlignment)
		{
		case eAT_Center:
			va = GFxMovieView::Align_Center;
			break;
		case eAT_TopCenter:
			va = GFxMovieView::Align_TopCenter;
			break;
		case eAT_BottomCenter:
			va = GFxMovieView::Align_BottomCenter;
			break;
		case eAT_CenterLeft:
			va = GFxMovieView::Align_CenterLeft;
			break;
		case eAT_CenterRight:
			va = GFxMovieView::Align_CenterRight;
			break;
		case eAT_TopLeft:
			va = GFxMovieView::Align_TopLeft;
			break;
		case eAT_TopRight:
			va = GFxMovieView::Align_TopRight;
			break;
		case eAT_BottomLeft:
			va = GFxMovieView::Align_BottomLeft;
			break;
		case eAT_BottomRight:
			va = GFxMovieView::Align_BottomRight;
			break;
		default:
			assert(0);
			va = GFxMovieView::Align_Center;
			break;
		}
		m_pMovieView->SetViewAlignment(va);
	}
}

IFlashPlayer::EAlignType CFlashPlayer::GetViewAlignment() const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	EAlignType viewAlignment = eAT_Center;
	if (m_pMovieView)
	{
		GFxMovieView::AlignType va = m_pMovieView->GetViewAlignment();
		switch (va)
		{
		case GFxMovieView::Align_Center:
			viewAlignment = eAT_Center;
			break;
		case GFxMovieView::Align_TopCenter:
			viewAlignment = eAT_TopCenter;
			break;
		case GFxMovieView::Align_BottomCenter:
			viewAlignment = eAT_BottomCenter;
			break;
		case GFxMovieView::Align_CenterLeft:
			viewAlignment = eAT_CenterLeft;
			break;
		case GFxMovieView::Align_CenterRight:
			viewAlignment = eAT_CenterRight;
			break;
		case GFxMovieView::Align_TopLeft:
			viewAlignment = eAT_TopLeft;
			break;
		case GFxMovieView::Align_TopRight:
			viewAlignment = eAT_TopRight;
			break;
		case GFxMovieView::Align_BottomLeft:
			viewAlignment = eAT_BottomLeft;
			break;
		case GFxMovieView::Align_BottomRight:
			viewAlignment = eAT_BottomRight;
			break;
		default:
			assert(0);
			viewAlignment = eAT_Center;
			break;
		}
	}
	return viewAlignment;
}

void CFlashPlayer::GetViewport(int& x0, int& y0, int& width, int& height, float& aspectRatio) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		x0 = viewport.Left;
		y0 = viewport.Top;
		width = viewport.Width;
		height = viewport.Height;
		aspectRatio = viewport.AspectRatio;
	}
	else
	{
		x0 = y0 = width = height = 0;
		aspectRatio = 1;
	}
}

void CFlashPlayer::SetScissorRect(int x0, int y0, int width, int height)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		viewport.SetScissorRect(x0, y0, width, height);
		m_pMovieView->SetViewport(viewport);
	}
}

void CFlashPlayer::GetScissorRect(int& x0, int& y0, int& width, int& height) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		if (viewport.Flags & GViewport::View_UseScissorRect)
		{
			x0 = viewport.ScissorLeft;
			y0 = viewport.ScissorTop;
			width = viewport.ScissorWidth;
			height = viewport.ScissorHeight;
			return;
		}
	}
	x0 = y0 = width = height = 0;
}

void CFlashPlayer::Advance(float deltaTime)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_1ARG(eFncAdvance, VOID_RETURN, "Advance", deltaTime);
	SET_LOG_CONTEXT(m_filePath);
	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Flash Advance %s", m_filePath->c_str());

	if (m_pMovieView)
	{
		UpdateASVerbosity();
		m_pMovieView->Advance(deltaTime);
	}
}

void CFlashPlayer::Render()
{
	//FLASH_PROFILER_LIGHT; // don't add profiling macro to this dispatch function (cost is neglectable and would be double counted if MT rendering is off), callback will measure real cost

	if (!IsFlashEnabled())
		return;

	gEnv->pRenderer->FlashRender(shared_from_this());
}

IScaleformPlayback* CFlashPlayer::GetPlayback()
{
	return m_pRenderer->GetPlayback();
}

void CFlashPlayer::RenderCallback(EFrameType ft)
{
	FLASH_PROFILER_LIGHT;

	MEMSTAT_CONTEXT_FMT(EMemStatContextTypes::MSC_Other, 0, "Flash RenderCallback %s", m_filePath->c_str());

	{
		SYNC_THREADS;
		//#if defined(ENABLE_FLASH_INFO)
		//		if (ms_sys_flash_info)
		//			pXRender->SF_Flush();
		//#endif
		{
			CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
			FLASH_PROFILE_FUNC(eFncDisplay, VOID_RETURN, "Display");
			SET_LOG_CONTEXT(m_filePath);
			bool devLost = false;
			gEnv->pRenderer->EF_Query(EFQ_DeviceLost, devLost);
			if (m_pMovieView && !devLost)
			{
				UpdateRenderFlags();
				m_pRenderer->SetClearFlags(m_clearFlags, m_clearColor);
				m_pRenderer->SetCompositingDepth(m_compDepth);
				m_pRenderer->SetStereoMode(ft != EFT_Mono, ft == EFT_StereoLeft);
				m_pRenderer->StereoEnforceFixedProjectionDepth(m_stereoFixedProjDepth);
				m_pRenderer->StereoSetCustomMaxParallax(m_stereoCustomMaxParallax);
				m_pRenderer->AvoidStencilClear(m_avoidStencilClear);
				m_pRenderer->EnableMaskedRendering(m_maskedRendering);
				m_pRenderer->ExtendCanvasToViewport(m_extendCanvasToVP);

				{
					SYNC_DISPLAY_BEGIN
					CAPTURE_DRAW_STATS_BEGIN

					m_pMovieView->Display();

					CAPTURE_DRAW_STATS_END
					SYNC_DISPLAY_END
				}
			}
			//#if defined(ENABLE_FLASH_INFO)
			//			if (ms_sys_flash_info)
			//				pXRender->SF_Flush();
			//#endif
		}
	}
}

void CFlashPlayer::RenderPlaybackLocklessCallback(int cbIdx, EFrameType ft, bool finalPlayback)
{
	FLASH_PROFILER_LIGHT;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Flash RenderPlaybackLocklessCallback");

	{
		//#if defined(ENABLE_FLASH_INFO)
		//		if (ms_sys_flash_info)
		//			pXRender->SF_Flush();
		//#endif
		{
			CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
			FLASH_PROFILE_FUNC(eFncDisplay, VOID_RETURN, "Display Lockless (Playback)");
			SET_LOG_CONTEXT(m_filePath);

			GRendererCommandBuffer& cmdBuf = m_cmdBuf[cbIdx];

			bool devLost = false;
			gEnv->pRenderer->EF_Query(EFQ_DeviceLost, devLost);
			if (!devLost)
			{
				// UpdateRenderFlags(); // not needed as we only play back the pre-recorded commands
				m_pRenderer->SetClearFlags(m_clearFlags, m_clearColor);
				m_pRenderer->SetCompositingDepth(m_compDepth);
				m_pRenderer->SetStereoMode(ft != EFT_Mono, ft == EFT_StereoLeft);
				m_pRenderer->StereoEnforceFixedProjectionDepth(m_stereoFixedProjDepth);
				m_pRenderer->StereoSetCustomMaxParallax(m_stereoCustomMaxParallax);
				m_pRenderer->AvoidStencilClear(m_avoidStencilClear);
				m_pRenderer->EnableMaskedRendering(m_maskedRendering);
				m_pRenderer->ExtendCanvasToViewport(m_extendCanvasToVP);
				CAPTURE_DRAW_STATS_BEGIN
				cmdBuf.Render(m_pRenderer->GetPlayback());
				CAPTURE_DRAW_STATS_END
			}

			if (finalPlayback)
			{
				IF (devLost, 0)
					cmdBuf.DropResourceRefs();
				cmdBuf.Reset(0);
			}

			//#if defined(ENABLE_FLASH_INFO)
			//			if (ms_sys_flash_info)
			//				pXRender->SF_Flush();
			//#endif
		}
	}
}

void CFlashPlayer::DummyRenderCallback(EFrameType ft)
{}

void CFlashPlayer::SetClearFlags(uint32 clearFlags, ColorF clearColor)
{
	//SYNC_THREADS; //don't sync (should hardly produce read/write conflicts and thus rendering glitches, if ever)

	m_clearFlags = clearFlags;
	m_clearColor = clearColor;
}

void CFlashPlayer::SetCompositingDepth(float depth)
{
	//SYNC_THREADS; //don't sync (should hardly produce read/write conflicts and thus rendering glitches, if ever)

	m_compDepth = depth;
}

void CFlashPlayer::StereoEnforceFixedProjectionDepth(bool enforce)
{
	//SYNC_THREADS; //don't sync (should hardly produce read/write conflicts and thus rendering glitches, if ever)

	m_stereoFixedProjDepth = enforce;
}

void CFlashPlayer::StereoSetCustomMaxParallax(float maxParallax)
{
	//SYNC_THREADS; //don't sync (should hardly produce read/write conflicts and thus rendering glitches, if ever)

	m_stereoCustomMaxParallax = maxParallax;
}

void CFlashPlayer::AvoidStencilClear(bool avoid)
{
	//SYNC_THREADS; //don't sync (should hardly produce read/write conflicts and thus rendering glitches, if ever)

	m_avoidStencilClear = avoid;
}

void CFlashPlayer::EnableMaskedRendering(bool enable)
{
	//SYNC_THREADS; //don't sync (should hardly produce read/write conflicts and thus rendering glitches, if ever)

	m_maskedRendering = enable;
}

void CFlashPlayer::ExtendCanvasToViewport(bool extend)
{
	//SYNC_THREADS; //don't sync (should hardly produce read/write conflicts and thus rendering glitches, if ever)

	m_extendCanvasToVP = extend;
}

void CFlashPlayer::Restart()
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		m_pMovieView->Restart();
	}
}

bool CFlashPlayer::IsPaused() const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	bool res = false;
	if (m_pMovieView)
	{
		res = m_pMovieView->IsPaused();
	}
	return res;
}

void CFlashPlayer::Pause(bool pause)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		m_pMovieView->SetPause(pause);
	}
}

void CFlashPlayer::GotoFrame(unsigned int frameNumber)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		m_pMovieView->GotoFrame(frameNumber);
	}
}

bool CFlashPlayer::GotoLabeledFrame(const char* pLabel, int offset)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	bool res = false;
	if (m_pMovieView && pLabel)
	{
		res = m_pMovieView->GotoLabeledFrame(pLabel, offset);
	}
	return res;
}

unsigned int CFlashPlayer::GetCurrentFrame() const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	unsigned int res = 0;
	if (m_pMovieView)
	{
		res = m_pMovieView->GetCurrentFrame();
	}
	return res;
}

bool CFlashPlayer::HasLooped() const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	bool res = false;
	if (m_pMovieView)
	{
		res = m_pMovieView->HasLooped();
	}
	return res;
}

void CFlashPlayer::SetFSCommandHandler(IFSCommandHandler* pHandler, void* pUserData)
{
	//SYNC_THREADS; //don't sync (handler needs to reside in same thread as flash player instance)

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	m_pFSCmdHandler = pHandler;
	m_pFSCmdHandlerUserData = pUserData;
}

void CFlashPlayer::SetExternalInterfaceHandler(IExternalInterfaceHandler* pHandler, void* pUserData)
{
	//SYNC_THREADS; //don't sync (handler needs to reside in same thread as flash player instance)

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	m_pEIHandler = pHandler;
	m_pEIHandlerUserData = pUserData;
}

void CFlashPlayer::SendCursorEvent(const SFlashCursorEvent& cursorEvent)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		switch (cursorEvent.m_state)
		{
		case SFlashCursorEvent::eCursorMoved:
			{
				GFxMouseEvent event(GFxEvent::MouseMove, (UInt) cursorEvent.m_button, (SInt16) cursorEvent.m_cursorX, (SInt16) cursorEvent.m_cursorY);
				m_pMovieView->HandleEvent(event);
				break;
			}
		case SFlashCursorEvent::eCursorPressed:
			{
				GFxMouseEvent event(GFxEvent::MouseDown, (UInt) cursorEvent.m_button, (SInt16) cursorEvent.m_cursorX, (SInt16) cursorEvent.m_cursorY);
				m_pMovieView->HandleEvent(event);
				break;
			}
		case SFlashCursorEvent::eCursorReleased:
			{
				GFxMouseEvent event(GFxEvent::MouseUp, (UInt) cursorEvent.m_button, (SInt16) cursorEvent.m_cursorX, (SInt16) cursorEvent.m_cursorY);
				m_pMovieView->HandleEvent(event);
				break;
			}
		case SFlashCursorEvent::eWheel:
			{
				GFxMouseEvent event(GFxEvent::MouseWheel, (UInt) cursorEvent.m_button, (SInt16) cursorEvent.m_cursorX, (SInt16) cursorEvent.m_cursorY, (Float) cursorEvent.m_wheelScrollVal);
				m_pMovieView->HandleEvent(event);
				break;
			}
		}
	}
}

void CFlashPlayer::SendKeyEvent(const SFlashKeyEvent& keyEvent)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GFxKeyEvent event(keyEvent.m_state == SFlashKeyEvent::eKeyDown ? GFxEvent::KeyDown : GFxEvent::KeyUp,
		                  (GFxKey::Code) keyEvent.m_keyCode, keyEvent.m_asciiCode, keyEvent.m_wcharCode);

		event.SpecialKeysState.SetShiftPressed((keyEvent.m_specialKeyState & SFlashKeyEvent::eShiftPressed) != 0);
		event.SpecialKeysState.SetCtrlPressed((keyEvent.m_specialKeyState & SFlashKeyEvent::eCtrlPressed) != 0);
		event.SpecialKeysState.SetAltPressed((keyEvent.m_specialKeyState & SFlashKeyEvent::eAltPressed) != 0);
		event.SpecialKeysState.SetCapsToggled((keyEvent.m_specialKeyState & SFlashKeyEvent::eCapsToggled) != 0);
		event.SpecialKeysState.SetNumToggled((keyEvent.m_specialKeyState & SFlashKeyEvent::eNumToggled) != 0);
		event.SpecialKeysState.SetScrollToggled((keyEvent.m_specialKeyState & SFlashKeyEvent::eScrollToggled) != 0);

		m_pMovieView->HandleEvent(event);
	}
}

void CFlashPlayer::SendCharEvent(const SFlashCharEvent& charEvent)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);

	if ((gEnv->pInput->GetModifiers() & eMM_Ctrl) != 0)
	{
		SFlashKeyEvent::EKeyCode code = static_cast<SFlashKeyEvent::EKeyCode>(SFlashKeyEvent::A + charEvent.m_wCharCode - 1);
		SFlashKeyEvent keyEvent(SFlashKeyEvent::eKeyDown, code, SFlashKeyEvent::eCtrlPressed, static_cast<unsigned char>(code), code);
		SendKeyEvent(keyEvent);

		return;
	}

	if (m_pMovieView)
	{
		GFxCharEvent event(charEvent.m_wCharCode, charEvent.m_keyboardIndex);

		m_pMovieView->HandleEvent(event);
	}
}

void CFlashPlayer::SetImeFocus()
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);

	CSharedFlashPlayerResources::GetAccess().SetImeFocus(m_pMovieView, true);
}

bool CFlashPlayer::HitTest(float x, float y) const
{
	return m_pMovieView && m_pMovieView->HitTest(x, y, GFxMovieView::HitTest_ShapesNoInvisible);
}

void CFlashPlayer::SetVisible(bool visible)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
		m_pMovieView->SetVisible(visible);
}

bool CFlashPlayer::GetVisible() const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	bool res(false);
	if (m_pMovieView)
		res = m_pMovieView->GetVisible();
	return res;
}

bool CFlashPlayer::SetOverrideTexture(const char* pResourceName, ITexture* pTexture, bool resize)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	if (pTexture)
	{
		GFxResource* pres = m_pMovieDef->GetResource(pResourceName);
		GFxImageResource* pimageRes = 0;
		if (pres && pres->GetResourceType() == GFxResource::RT_Image)
			pimageRes = (GFxImageResource*)pres;

		if (pimageRes)
		{
			GImageInfoBase* pPrev = pimageRes->GetImageInfo();
			GImageInfoBase* pimageInfo = resize
			                             ? new GImageInfoTextureXRender(pTexture, pPrev->GetWidth(), pPrev->GetHeight())
			                             : new GImageInfoTextureXRender(pTexture);
			pimageRes->SetImageInfo(pimageInfo);
			return true;
		}
	}
	return false;
}

bool CFlashPlayer::SetVariable(const char* pPathToVar, const SFlashVarValue& value)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_2ARG(eFncSetVar, false, "SetVariable", pPathToVar, &value);
	SET_LOG_CONTEXT(m_filePath);
	bool res(false);
	if (m_pMovieView)
		res = m_pMovieView->SetVariable(pPathToVar, ConvertValue(value));
	return res;
}

bool CFlashPlayer::SetVariable(const char* pPathToVar, const IFlashVariableObject* pVarObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_2ARG(eFncSetVar, false, "SetVariable", pPathToVar, (void*) pVarObj);
	SET_LOG_CONTEXT(m_filePath);
	bool res(false);
	if (m_pMovieView)
		res = m_pMovieView->SetVariable(pPathToVar, pVarObj ? GetGFxValue(pVarObj) : GFxValue());
	return res;
}

bool CFlashPlayer::GetVariable(const char* pPathToVar, SFlashVarValue& value) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_2ARG(eFncGetVar, false, "GetVariable", pPathToVar, (void*) &value);
	SET_LOG_CONTEXT(m_filePath);
	m_retValRefHolder = GFxValue();
	bool res(false);
	if (m_pMovieView)
	{
		GFxValue retVal;
		res = m_pMovieView->GetVariable(&retVal, pPathToVar);
		if (res && (retVal.IsString() || retVal.IsStringW()))
			m_retValRefHolder = retVal;
		value = ConvertValue(retVal);
	}
	return res;
}

bool CFlashPlayer::GetVariable(const char* pPathToVar, IFlashVariableObject*& pVarObj) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_2ARG(eFncGetVar, false, "GetVariable", pPathToVar, (void*) pVarObj);
	SET_LOG_CONTEXT(m_filePath);
	pVarObj = 0;
	if (m_pMovieView)
	{
		GFxValue retVal;
		if (m_pMovieView->GetVariable(&retVal, pPathToVar))
		{
			//assert(retVal.IsObject());
			pVarObj = new CFlashVariableObject(retVal, m_filePath, m_lock);
		}
	}
	return pVarObj != 0;
}

bool CFlashPlayer::IsAvailable(const char* pPathToVar) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_1ARG(eFncIsAvailable, false, "IsAvailable", pPathToVar);
	SET_LOG_CONTEXT(m_filePath);
	bool res(false);
	if (m_pMovieView)
		res = m_pMovieView->IsAvailable(pPathToVar);
	return res;
}

bool CFlashPlayer::SetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, const void* pData, unsigned int count)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_5ARG(eFncSetVar, false, "SetVariableArray", type, pPathToVar, index, pData, count);
	SET_LOG_CONTEXT(m_filePath);
	GFxMovie::SetArrayType translatedType;
	bool res(false);
	if (m_pMovieView && GetArrayType(type, translatedType))
		res = m_pMovieView->SetVariableArray(translatedType, pPathToVar, index, pData, count);
	return res;
}

unsigned int CFlashPlayer::GetVariableArraySize(const char* pPathToVar) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_1ARG(eFncGetVar, 0, "GetVariableArraySize", pPathToVar);
	SET_LOG_CONTEXT(m_filePath);
	int res(0);
	if (m_pMovieView)
		res = m_pMovieView->GetVariableArraySize(pPathToVar);
	return res;
}

bool CFlashPlayer::GetVariableArray(EFlashVariableArrayType type, const char* pPathToVar, unsigned int index, void* pData, unsigned int count) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_5ARG(eFncGetVar, false, "GetVariableArray", type, pPathToVar, index, pData, count);
	SET_LOG_CONTEXT(m_filePath);
	bool res(false);
	if (m_pMovieView && pData)
	{
		GFxMovie::SetArrayType translatedType;
		if (GetArrayType(type, translatedType))
			res = m_pMovieView->GetVariableArray(translatedType, pPathToVar, index, pData, count);
	}
	return res;
}

bool CFlashPlayer::Invoke(const char* pMethodName, const SFlashVarValue* pArgs, unsigned int numArgs, SFlashVarValue* pResult)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	FLASH_PROFILE_FUNC_1ARG_VALIST(eFncInvoke, false, "Invoke", pMethodName, pArgs, numArgs);
	SET_LOG_CONTEXT(m_filePath);
	m_retValRefHolder = GFxValue();
	bool res(false);
	if (m_pMovieView && (pArgs || !numArgs))
	{
		assert(!pArgs || numArgs);
		GFxValue* pTranslatedArgs(0);
		if (pArgs && numArgs)
		{
			PREFAST_SUPPRESS_WARNING(6255)
			pTranslatedArgs = (GFxValue*) alloca(numArgs * sizeof(GFxValue));
			if (pTranslatedArgs)
			{
				for (unsigned int i(0); i < numArgs; ++i)
					new(&pTranslatedArgs[i])GFxValue(ConvertValue(pArgs[i]));
			}
		}

		if (pTranslatedArgs || !numArgs)
		{
			GFxValue retVal;
			{
				// This is needed to disable fp exception being triggered in Scaleform
				CScopedFloatingPointException fpExceptionScope(eFPE_None);

				res = m_pMovieView->Invoke(pMethodName, &retVal, pTranslatedArgs, numArgs);
			}
			if (pResult)
			{
				if (retVal.IsString() || retVal.IsStringW())
					m_retValRefHolder = retVal;
				*pResult = ConvertValue(retVal);
			}
			for (unsigned int i(0); i < numArgs; ++i)
				pTranslatedArgs[i].~GFxValue();
		}
	}
	return res;
}

bool CFlashPlayer::CreateString(const char* pString, IFlashVariableObject*& pVarObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	pVarObj = 0;
	if (m_pMovieView)
	{
		GFxValue retVal;
		m_pMovieView->CreateString(&retVal, pString);
		if (retVal.IsString())
			pVarObj = new CFlashVariableObject(retVal, m_filePath, m_lock);
	}
	return pVarObj != 0;
}

bool CFlashPlayer::CreateStringW(const wchar_t* pString, IFlashVariableObject*& pVarObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	pVarObj = 0;
	if (m_pMovieView)
	{
		GFxValue retVal;
		m_pMovieView->CreateStringW(&retVal, pString);
		if (retVal.IsStringW())
			pVarObj = new CFlashVariableObject(retVal, m_filePath, m_lock);
	}
	return pVarObj != 0;
}

bool CFlashPlayer::CreateObject(const char* pClassName, const SFlashVarValue* pArgs, unsigned int numArgs, IFlashVariableObject*& pVarObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	pVarObj = 0;
	if (m_pMovieView && (pArgs || !numArgs))
	{
		assert(!pArgs || numArgs);
		GFxValue* pTranslatedArgs(0);
		if (pArgs && numArgs)
		{
			PREFAST_SUPPRESS_WARNING(6255)
			pTranslatedArgs = (GFxValue*) alloca(numArgs * sizeof(GFxValue));
			if (pTranslatedArgs)
			{
				for (unsigned int i(0); i < numArgs; ++i)
					new(&pTranslatedArgs[i])GFxValue(ConvertValue(pArgs[i]));
			}
		}

		if (pTranslatedArgs || !numArgs)
		{
			GFxValue retVal;
			m_pMovieView->CreateObject(&retVal, pClassName, pTranslatedArgs, numArgs);
			if (retVal.IsObject())
				pVarObj = new CFlashVariableObject(retVal, m_filePath, m_lock);
			for (unsigned int i(0); i < numArgs; ++i)
				pTranslatedArgs[i].~GFxValue();
		}
	}
	return pVarObj != 0;
}

bool CFlashPlayer::CreateArray(IFlashVariableObject*& pVarObj)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	pVarObj = 0;
	if (m_pMovieView)
	{
		GFxValue retVal;
		m_pMovieView->CreateArray(&retVal);
		if (retVal.IsArray())
			pVarObj = new CFlashVariableObject(retVal, m_filePath, m_lock);
	}
	return pVarObj != 0;
}

struct FunctionHandlerAdaptor : public GFxFunctionHandler
{
	struct ReturnValue : public IActionScriptFunction::IReturnValue
	{
		ReturnValue(GFxMovieView* pMovieView)
			: m_pMovieView(pMovieView)
			, m_value()
		{
		}

		~ReturnValue()
		{
		}

		void Set(const IFlashVariableObject* value)
		{
			assert(value);
			m_value = GetGFxValue(value);
		}

		void Set(const SFlashVarValue& value, bool createManagedValue = true)
		{
			switch (value.GetType())
			{
			case SFlashVarValue::eConstStrPtr:
				{
					if (createManagedValue)
						m_pMovieView->CreateString(&m_value, value.GetConstStrPtr());
					else
						m_value = ConvertValue(value);
					break;
				}
			case SFlashVarValue::eConstWstrPtr:
				{
					if (createManagedValue)
						m_pMovieView->CreateStringW(&m_value, value.GetConstWstrPtr());
					else
						m_value = ConvertValue(value);
					break;
				}
			default:
				m_value = ConvertValue(value);
				break;
			}
		}

		GFxMovieView* m_pMovieView;
		GFxValue      m_value;
	};

	FunctionHandlerAdaptor(IActionScriptFunction* pFunc, CFlashPlayer* pPlayer)
		: m_pFunc(pFunc)
		, m_pPlayer(pPlayer)
	{
		assert(m_pFunc);
		assert(m_pPlayer);
	}

	~FunctionHandlerAdaptor()
	{
	}

	void Call(const GFxFunctionHandler::Params& params)
	{
		// translate this and arguments
		const unsigned int numArgs = params.ArgCount;

		PREFAST_SUPPRESS_WARNING(6255)
		CFlashVariableObject * pTranslatedArgsWithThisRef = (CFlashVariableObject*) alloca((numArgs + 1) * sizeof(CFlashVariableObject));
		assert(pTranslatedArgsWithThisRef);

		assert(params.pArgsWithThisRef);
		for (unsigned int i = 0; i < numArgs + 1; ++i)
			new(&pTranslatedArgsWithThisRef[i])CFlashVariableObject(params.pArgsWithThisRef[i], m_pPlayer->m_filePath, m_pPlayer->m_lock);

		const IFlashVariableObject** ppTranslatedArgs = 0;
		if (numArgs)
		{
			PREFAST_SUPPRESS_WARNING(6255)
			ppTranslatedArgs = (const IFlashVariableObject**) alloca(numArgs * sizeof(IFlashVariableObject*));
			assert(ppTranslatedArgs);
			for (unsigned int i = 0; i < numArgs; ++i)
				ppTranslatedArgs[i] = &pTranslatedArgsWithThisRef[i + 1];
		}

		// setup translated calling parameters
		IActionScriptFunction::Params translatedParams;
		translatedParams.pFromPlayer = m_pPlayer;
		translatedParams.pUserData = params.pUserData;
		translatedParams.pThis = &pTranslatedArgsWithThisRef[0];
		translatedParams.pArgs = ppTranslatedArgs;
		translatedParams.numArgs = numArgs;

		// call function
		assert(params.pMovie);
		ReturnValue retVal(params.pMovie);

		m_pFunc->Call(translatedParams, &retVal);

		// translate return value
		if (params.pRetVal)
			*params.pRetVal = retVal.m_value;

		// destruct translated this and arguments
		for (unsigned int i = 0; i < numArgs + 1; ++i)
			pTranslatedArgsWithThisRef[i].~CFlashVariableObject();
	}

	IActionScriptFunction* m_pFunc;
	CFlashPlayer*          m_pPlayer;
};

bool CFlashPlayer::CreateFunction(IFlashVariableObject*& pFuncVarObj, IActionScriptFunction* pFunc, void* pUserData)
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	pFuncVarObj = 0;
	if (m_pMovieView && pFunc)
	{
		GPtr<FunctionHandlerAdaptor> fhb = *(new FunctionHandlerAdaptor(pFunc, this));
		GFxValue retVal;
		m_pMovieView->CreateFunction(&retVal, fhb, pUserData);
		if (retVal.IsObject())
			pFuncVarObj = new CFlashVariableObject(retVal, m_filePath, m_lock);
	}
	return pFuncVarObj != 0;
}

unsigned int CFlashPlayer::GetFrameCount() const
{
	return m_frameCount;
}

float CFlashPlayer::GetFrameRate() const
{
	return m_frameRate;
}

int CFlashPlayer::GetWidth() const
{
	return m_width;
}

int CFlashPlayer::GetHeight() const
{
	return m_height;
}

size_t CFlashPlayer::GetMetadata(char* pBuff, unsigned int buffSize) const
{
	FLASH_PROFILER_LIGHT;

	//SYNC_THREADS; // should be safe as meta data for the root is only assigned during Load()

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	size_t res(0);
	if (m_pMovieDef)
		res = m_pMovieDef->GetMetadata(pBuff, buffSize);
	return res;
}

bool CFlashPlayer::HasMetadata(const char* pTag) const
{
	FLASH_PROFILER_LIGHT;

	//SYNC_THREADS; // should be safe as meta data for the root is only assigned during Load()

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);

	bool bResult = false;
	const size_t buffSize = GetMetadata(0, 0);
	if (buffSize)
	{
		char* pMetaData = new char[buffSize];
		if (pMetaData)
		{
			GetMetadata(pMetaData, buffSize);
			bResult = CryStringUtils::stristr(pMetaData, pTag) != 0;
		}
		delete[] pMetaData;
	}

	return bResult;
}

const char* CFlashPlayer::GetFilePath() const
{
	//SYNC_THREADS; // should be safe as m_filePath is only modified during Load()

	//CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	return m_filePath->c_str();
}

void CFlashPlayer::ResetDirtyFlags()
{
	CryGFxTranslator::GetAccess().SetDirty(false);
}

void CFlashPlayer::ScreenToClient(int& x, int& y) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		x -= viewport.Left;
		y -= viewport.Top;
	}
	else
	{
		x = y = 0;
	}
}

void CFlashPlayer::ClientToScreen(int& x, int& y) const
{
	FLASH_PROFILER_LIGHT;

	SYNC_THREADS;

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	SET_LOG_CONTEXT(m_filePath);
	if (m_pMovieView)
	{
		GViewport viewport;
		m_pMovieView->GetViewport(&viewport);
		x += viewport.Left;
		y += viewport.Top;
	}
}

	#if defined(ENABLE_DYNTEXSRC_PROFILING)
void CFlashPlayer::LinkDynTextureSource(const struct IDynTextureSource* pDynTexSrc)
{
		#if defined(ENABLE_FLASH_INFO)
	if (!m_pProfilerData)
		m_pProfilerData = new SFlashProfilerData;
		#endif
	m_pDynTexSrc = pDynTexSrc;
}
	#endif

void CFlashPlayer::DelegateFSCommandCallback(const char* pCommand, const char* pArgs)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	// delegate action script command to client
	if (m_pFSCmdHandler)
	{
		m_pFSCmdHandler->HandleFSCommand(pCommand, pArgs, m_pFSCmdHandlerUserData);
	}
}

void CFlashPlayer::DelegateExternalInterfaceCallback(const char* pMethodName, const GFxValue* pArgs, UInt numArgs)
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	if (m_pMovieView)
	{
		// delegate action script command to client
		GFxValue retVal(GFxValue::VT_Undefined);
		if (m_pEIHandler)
		{
			assert(!pArgs || numArgs);
			SFlashVarValue* pTranslatedArgs(0);
			if (pArgs && numArgs)
			{
				PREFAST_SUPPRESS_WARNING(6255)
				pTranslatedArgs = (SFlashVarValue*) alloca(numArgs * sizeof(SFlashVarValue));
				if (pTranslatedArgs)
				{
					for (unsigned int i(0); i < numArgs; ++i)
						new(&pTranslatedArgs[i])SFlashVarValue(ConvertValue(pArgs[i]));
				}
			}

			if (pTranslatedArgs || !numArgs)
			{
				SFlashVarValue ret(SFlashVarValue::CreateUndefined());
				m_pEIHandler->HandleExternalInterfaceCall(pMethodName, pTranslatedArgs, numArgs, m_pEIHandlerUserData, &ret);
				retVal = ConvertValue(ret);
				for (unsigned int i(0); i < numArgs; ++i)
					pTranslatedArgs[i].~SFlashVarValue();
			}
		}
		m_pMovieView->SetExternalInterfaceRetVal(retVal);
	}
}

bool CFlashPlayer::IsFlashEnabled()
{
	return ms_sys_flash != 0;
}

bool CFlashPlayer::IsEdgeAaAllowed() const
{
	return m_allowEgdeAA && (ms_sys_flash_edgeaa != 0);
}

void CFlashPlayer::UpdateRenderFlags()
{
	const unsigned int renderFlags(m_renderConfig.GetRenderFlags());

	unsigned int newRenderFlags(renderFlags);
	newRenderFlags &= ~GFxRenderConfig::RF_EdgeAA;
	if (IsEdgeAaAllowed())
		newRenderFlags |= GFxRenderConfig::RF_EdgeAA;

	if (newRenderFlags != renderFlags)
		m_renderConfig.SetRenderFlags(newRenderFlags);

	float curMaxCurveError(m_renderConfig.GetMaxCurvePixelError());
	if (ms_sys_flash_curve_tess_error != curMaxCurveError)
	{
		if (ms_sys_flash_curve_tess_error < 1.0f)
			ms_sys_flash_curve_tess_error = 1.0f;
		m_renderConfig.SetMaxCurvePixelError(ms_sys_flash_curve_tess_error);
	}
}

void CFlashPlayer::UpdateASVerbosity()
{
	m_asVerbosity.SetActionFlags((ms_sys_flash_log_options& CFlashPlayer::LO_ACTIONSCRIPT) ? GFxActionControl::Action_Verbose : 0);
}

size_t CFlashPlayer::GetCommandBufferSize() const
{
	return m_cmdBuf.GetBufferSize();
}

	#if defined(ENABLE_FLASH_INFO)

		#include <CryRenderer/IRenderAuxGeom.h>

static void Draw2dLabel(float x, float y, float fontSize, const float* pColor, const char* pText)
{
	IRenderAuxText::DrawText(Vec3(x, y, 0.5f), fontSize, pColor, eDrawText_2D | eDrawText_800x600 | eDrawText_FixedSize | eDrawText_Monospace, pText);
}

static void DrawText(float x, float y, const float* pColor, const char* pFormat, ...)
{
	char buffer[512];

	va_list args;
	va_start(args, pFormat);
	cry_vsprintf(buffer, pFormat, args);
	va_end(args);

	Draw2dLabel(x, y, 1.4f, pColor, buffer);
}

static void DrawTextNoFormat(float x, float y, float* const pColor, const char* pText)
{
	Draw2dLabel(x, y, 1.4f, pColor, pText);
}

static inline float CalculateFlashVarianceFactor(float value, float variance)
{
	const float VAL_EPSILON(0.000001f);
	const float VAR_MULTIPLIER(2.0f);

	//variance = fabs(variance - value*value);
	float difference((float)sqrt_tpl(variance));

	value = (float) fabs(value);
	if (value < VAL_EPSILON)
		return 0;

	float factor(0);
	if (value > 0.01f)
		factor = (difference / value) * VAR_MULTIPLIER;

	return factor;
}

static void CalculateColor(float factor, float* pOutColor)
{
	float ColdColor[4] = { 0.15f, 0.9f, 0.15f, 1 };
	float HotColor[4] = { 1, 1, 1, 1 };

	for (int k = 0; k < 3; ++k)
		pOutColor[k] = HotColor[k] * factor + ColdColor[k] * (1.0f - factor);
	pOutColor[3] = 1;
}

static void CalculateColor(float value, float variance, float* pOutColor)
{
	CalculateColor(clamp_tpl(CalculateFlashVarianceFactor(value, variance), 0.0f, 1.0f), pOutColor);
}

static float CalculateVariance(float value, float avg)
{
	return (value - avg) * (value - avg);
}

static size_t InKB(size_t memSize)
{
	return (memSize + 1023) >> 10;
}

class CFlashInfoInput : public IInputEventListener
{
	// IInputEvenListener
public:
	virtual bool OnInputEvent(const SInputEvent& event)
	{
		if (event.state == eIS_Pressed)
		{
			switch (event.keyId)
			{
			case eKI_Space:
			case eKI_NP_Enter:
			case eKI_Enter:
				m_disable = true;
				break;
			case eKI_Left:
				m_close = true;
				break;
			case eKI_Right:
				m_open = true;
				break;
			case eKI_Down:
				m_moveDown = true;
				break;
			case eKI_Up:
				m_moveUp = true;
				break;
			case eKI_PgDn:
				m_scrollDown = true;
				break;
			case eKI_PgUp:
				m_scrollUp = true;
				break;
			case eKI_End:
				m_sort = true;
				break;
			case eKI_Home:
				m_pause = true;
				break;
			case eKI_Backslash:
				m_dim = true;
				break;
			case eKI_Insert:
				m_freeze = true;
				break;
			case eKI_Delete:
				m_showStrippedPath = true;
				break;
			}
		}
		return false;
	}

public:
	static CFlashInfoInput* Get()
	{
		static char s_flashInfoInputStorage[sizeof(CFlashInfoInput)] = { 0 };
		if (!ms_pInst)
			ms_pInst = new(s_flashInfoInputStorage) CFlashInfoInput;
		return ms_pInst;
	}

	static void Reset()
	{
		if (ms_pInst)
		{
			ms_pInst->~CFlashInfoInput();
			ms_pInst = 0;
		}
	}

public:
	bool OpenRequested()
	{
		bool ret = m_open;
		m_open = false;
		return ret;
	}

	bool CloseRequested()
	{
		bool ret = m_close;
		m_close = false;
		return ret;
	}

	bool ToggleDisable()
	{
		bool ret = m_disable;
		m_disable = false;
		return ret;
	}

	bool ShouldMoveDown()
	{
		bool ret = m_moveDown;
		m_moveDown = false;
		return ret;
	}

	bool ShouldMoveUp()
	{
		bool ret = m_moveUp;
		m_moveUp = false;
		return ret;
	}

	bool ShouldScrollDown()
	{
		bool ret = m_scrollDown;
		m_scrollDown = false;
		return ret;
	}

	bool ShouldScrollUp()
	{
		bool ret = m_scrollUp;
		m_scrollUp = false;
		return ret;
	}

	bool ShouldSort()
	{
		bool ret = m_sort;
		m_sort = false;
		return ret;
	}

	bool TogglePause()
	{
		bool ret = m_pause;
		m_pause = false;
		return ret;
	}

	bool ToggleDim()
	{
		bool ret = m_dim;
		m_dim = false;
		return ret;
	}

	bool ToggleFreeze()
	{
		bool ret = m_freeze;
		m_freeze = false;
		return ret;
	}

	bool ToggleShowStrippedPath()
	{
		bool ret = m_showStrippedPath;
		m_showStrippedPath = false;
		return ret;
	}

private:
	CFlashInfoInput()
		: m_disable(false)
		, m_open(false)
		, m_close(false)
		, m_moveDown(false)
		, m_moveUp(false)
		, m_scrollDown(false)
		, m_scrollUp(false)
		, m_sort(false)
		, m_pause(false)
		, m_dim(false)
		, m_freeze(false)
		, m_showStrippedPath(false)
	{
		if (gEnv->pInput)
			gEnv->pInput->AddConsoleEventListener(this);
	}

	virtual ~CFlashInfoInput()
	{
		if (gEnv->pInput)
			gEnv->pInput->RemoveConsoleEventListener(this);
	}

private:
	static CFlashInfoInput* ms_pInst;

private:
	bool m_disable;
	bool m_open;
	bool m_close;
	bool m_moveDown;
	bool m_moveUp;
	bool m_scrollDown;
	bool m_scrollUp;
	bool m_sort;
	bool m_pause;
	bool m_dim;
	bool m_freeze;
	bool m_showStrippedPath;
};

CFlashInfoInput* CFlashInfoInput::ms_pInst = 0;

	#endif // #if defined(ENABLE_FLASH_INFO)

void CFlashPlayer::RenderFlashInfo()
{
	#if defined(ENABLE_FLASH_INFO)
	CryAutoCriticalSection lock(GetList().GetLock());

	IRenderer* pRenderer = gEnv->pRenderer;
	assert(pRenderer);

	const int curFrameID = pRenderer->GetFrameID(false);
	CFlashFunctionProfilerLog::GetAccess().Enable(ms_sys_flash_info != 0 && (ms_sys_flash_log_options & LO_PEAKS) != 0, curFrameID);

	IScaleformRecording* pFlashRenderer = CSharedFlashPlayerResources::GetAccess().GetRenderer(true);
	GFxLoader2* pLoader = CSharedFlashPlayerResources::GetAccess().GetLoader(true);
	GMemoryHeap* pGFxHeap = GMemory::GetGlobalHeap();

	static uint32 s_lastNumTexturesTotal = GTextureXRenderBase::GetNumTexturesTotal();

	if (ms_sys_flash_info)
	{
		IF (ms_sys_flash_info == 3, 0)
		{
			int fontCacheTexId = GTextureXRenderBase::GetFontCacheTextureID();
			if (pRenderer->EF_GetTextureByID(fontCacheTexId))
			{
				//pRenderer->SetState(GS_NODEPTHTEST);
				IRenderAuxImage::Draw2dImage(0, 0, 450, 450, -1, 0.0f, 1.0f, 1.0f, 0.0f);
				//pRenderer->SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
				//pRenderer->Draw2dImageStretchMode(true);
				IRenderAuxImage::Draw2dImage(0, 0, 450, 450, fontCacheTexId, 0.0f, 1.0f, 1.0f, 0.0f);
				//pRenderer->Draw2dImageStretchMode(false);
			}
			return;
		}

		{
			static bool s_dimBG = false;
			if (CFlashInfoInput::Get()->ToggleDim())
				s_dimBG = !s_dimBG;
			if (s_dimBG)
			{
				//pRenderer->SetState(GS_NODEPTHTEST | GS_BLSRC_SRCALPHA | GS_BLDST_ONEMINUSSRCALPHA);
				IRenderAuxImage::Draw2dImage(0, 0, 800, 600, -1, 0.0f, 1.0f, 1.0f, 0.0f, 0, 0, 0, 0, 0.75f, 1);
			}
		}

		// define color
		const float inactiveColChannelMult(0.7f);
		static const float color[4] = { 1, 1, 1, 1 };
		static const float colorList[4] = { 0.3f, 0.8f, 1, 1 };
		static const float colorListInactive[4] = { 0.6f, 0.6f, 0.6f, 1 };
		static const float colorListSelected[4] = { 1, 0, 0, 1 };
		static const float colorListSelectedInactive[4] = { 0.6f, 0, 0, 1 };

		float xAdj = 0.0f;
		float yAdj = 0.0f;

		IF (ms_sys_flash_info == 4 || ms_sys_flash_info == 5, 0)
		{
			struct PrintHeapInfo : public GMemoryHeap::HeapVisitor
			{
				PrintHeapInfo(float x, float y, float limitY, unsigned int startIdx, bool logToConsole)
					: m_x(x), m_y(y), m_limitY(limitY), m_sumTotalUsed(0), m_sumTotalFoot(0), m_startIdx(startIdx), m_idx(0), m_overPrintLimit(false), m_logToConsole(logToConsole) {}

				virtual void Visit(GMemoryHeap* /*pParent*/, GMemoryHeap* pChild)
				{
					assert(pChild);

					size_t totalUsed = pChild->GetTotalUsedSpace();
					size_t totalFoot = pChild->GetTotalFootprint();

					m_overPrintLimit = (m_y + 14.0f) > m_limitY;
					if (m_idx >= m_startIdx && !m_overPrintLimit)
					{
						const float* col = (m_idx & 1) ? color : colorListInactive;
						DrawText(m_x, m_y, col, "Used (kb): %5d\tFootprint (kb): %5d\t[Heap] %s", InKB(totalUsed), InKB(totalFoot), pChild->GetName());
						m_y += 14.0f;
					}
					++m_idx;

					if (m_logToConsole)
						gEnv->pLog->Log("Used (kb): %5" PRISIZE_T "\tFootprint (kb): %5" PRISIZE_T "\t[Heap] %s", InKB(totalUsed), InKB(totalFoot), pChild->GetName());

					m_sumTotalUsed += totalUsed;
					m_sumTotalFoot += totalFoot;
				}

				size_t GetSumTotalUsed() const  { return m_sumTotalUsed; }
				size_t GetSumTotalFoot() const  { return m_sumTotalFoot; }

				bool   OverPrintLimit() const   { return m_overPrintLimit; }
				float  GetLastPrintPosY() const { return m_y - 14.0f; }

			protected:
				float              m_x, m_y;
				const float        m_limitY;
				size_t             m_sumTotalUsed, m_sumTotalFoot;
				const unsigned int m_startIdx;
				unsigned int       m_idx;
				bool               m_overPrintLimit;
				const bool         m_logToConsole;
			};

			GMemoryHeap* pHeap = 0;
		#if defined(USE_GFX_VIDEO)
			if (ms_sys_flash_info == 5)
			{
				GPtr<GFxVideoBase> pVideo = pLoader ? pLoader->GetVideo() : 0;
				pHeap = pVideo ? pVideo->GetHeap() : 0;
			}
			else
		#endif // #if defined(USE_GFX_VIDEO)
			pHeap = pGFxHeap;

			if (pHeap)
			{
				static unsigned int s_startIdx = 0;
				if (CFlashInfoInput::Get()->ShouldMoveUp() && s_startIdx > 0)
					--s_startIdx;
				else if (CFlashInfoInput::Get()->ShouldMoveDown())
					++s_startIdx;
				else if (CFlashInfoInput::Get()->ShouldScrollUp())
					s_startIdx = s_startIdx > 5 ? s_startIdx - 5 : 0;
				else if (CFlashInfoInput::Get()->ShouldScrollDown())
					s_startIdx += 5;
				;

				size_t totalUsed = pHeap->GetTotalUsedSpace();
				size_t totalFoot = pHeap->GetTotalFootprint();

				const bool logToConsole = CFlashInfoInput::Get()->ToggleDisable();

				DrawText(xAdj + 10.0f, yAdj + 10.0f, colorList, "Used (kb): %5d\tFootprint (kb): %5d\t[Heap] %s", InKB(totalUsed), InKB(totalFoot), pHeap->GetName());
				if (logToConsole)
					gEnv->pLog->Log("Used (kb): %5" PRISIZE_T "\tFootprint (kb): %5" PRISIZE_T "\t[Heap] %s", InKB(totalUsed), InKB(totalFoot), pHeap->GetName());

				PrintHeapInfo phi(xAdj + 10.0f, yAdj + 48.0f, (float) pRenderer->GetOverlayHeight(), s_startIdx, logToConsole);
				pHeap->VisitChildHeaps(&phi);

				DrawText(xAdj + 10.0f, yAdj + 24.0f, colorList, "Used (kb): %5" PRISIZE_T "\tFootprint (kb): %5" PRISIZE_T "\t[Heap] %s (self)", InKB(totalUsed - phi.GetSumTotalUsed()), InKB(totalFoot - phi.GetSumTotalFoot()), pHeap->GetName());
				if (logToConsole)
					gEnv->pLog->Log("Used (kb): %5" PRISIZE_T "\tFootprint (kb): %5" PRISIZE_T "\t[Heap] %s (self)", InKB(totalUsed - phi.GetSumTotalUsed()), InKB(totalFoot - phi.GetSumTotalFoot()), pHeap->GetName());

				if (s_startIdx > 0)
					DrawText(xAdj, yAdj + 48.0f, colorListSelected, "^");
				if (phi.OverPrintLimit())
					DrawText(xAdj, phi.GetLastPrintPosY(), colorListSelected, "v");
			}
			return;
		}

		bool isMTRenderer = false;
		pRenderer->EF_Query(EFQ_RenderMultithreaded, isMTRenderer);

		// display number of primitives rendered
		GRenderer::Stats stats;
		if (pFlashRenderer)
			pFlashRenderer->GetRenderStats(&stats, true);
		DrawText(xAdj + 10.0f, yAdj + 10.0f, color, "#Tris      : %5d", stats.Triangles);
		DrawText(xAdj + 10.0f, yAdj + 22.0f, color, "#Lines     : %5d", stats.Lines);
		DrawText(xAdj + 10.0f, yAdj + 34.0f, color, "#Masks     : %5d", stats.Masks);
		DrawText(xAdj + 10.0f, yAdj + 46.0f, color, "#DrawCalls : %5d", stats.Primitives);
		DrawText(xAdj + 10.0f, yAdj + 58.0f, color, "#Filters   : %5d", stats.Filters);

		// display memory statistics
		if (pGFxHeap)
		{
			GPtr<GFxMeshCacheManager> pMeshCacheMgr = pLoader ? pLoader->GetMeshCacheManager() : 0;
			GMemoryHeap* pHeap = pMeshCacheMgr ? pMeshCacheMgr->GetHeap() : 0;
			size_t memMeshCache = pHeap ? pHeap->GetTotalUsedSpace() : 0;

			GPtr<GFxFontCacheManager> pFontCacheMgr = pLoader ? pLoader->GetFontCacheManager() : 0;
			pHeap = pFontCacheMgr ? pFontCacheMgr->GetHeap() : 0;
			size_t memFontCache = pHeap ? pHeap->GetTotalUsedSpace() : 0;

			GSysAllocCryMem::Stats memAllocStats = CSharedFlashPlayerResources::GetAccess().GetSysAllocStats();
			DrawText(xAdj + 10.0f, yAdj + 77.0f, color, "Sys #Alloc() : %6d", memAllocStats.AllocCount);
			DrawText(xAdj + 10.0f, yAdj + 89.0f, color, "Sys #Free()  : %6d", memAllocStats.FreeCount);
			DrawText(xAdj + 10.0f, yAdj + 110.0f, color, "Sys Allocated        : %6dk", InKB(memAllocStats.Allocated));
			DrawText(xAdj + 10.0f, yAdj + 122.0f, color, "GlobalHeap Footprint : %6dk", InKB(pGFxHeap->GetTotalFootprint()));
			DrawText(xAdj + 10.0f, yAdj + 134.0f, color, "GlobalHeap Used      : %6dk", InKB(pGFxHeap->GetTotalUsedSpace()));
			DrawText(xAdj + 10.0f, yAdj + 146.0f, color, "MeshCacheHeap Used   : %6dk", InKB(memMeshCache));
			DrawText(xAdj + 10.0f, yAdj + 158.0f, color, "FontCacheHeap Used   : %6dk", InKB(memFontCache));
			DrawText(xAdj + 10.0f, yAdj + 170.0f, color, "TexMemory Used       : %6dk", InKB(GTextureXRenderBase::GetTextureMemoryUsed()));

			DrawText(xAdj + 300.0f, yAdj + 96.0f, color, "Sys heap frag: %.2f%%", CSharedFlashPlayerResources::GetAccess().GetFlashHeapFragmentation() * 100.0f);
			DrawText(xAdj + 500.0f, yAdj + 96.0f, color, "Custom arenas active: %s", CSharedFlashPlayerResources::GetAccess().AreCustomMemoryArenasActive() ? "yes" : "no");

			uint32 numTexturesTotal = GTextureXRenderBase::GetNumTexturesTotal() + GTextureXRenderBase::GetNumTexturesTempRT();
			bool texturesAtRuntime = numTexturesTotal > s_lastNumTexturesTotal;
			if (texturesAtRuntime)
				s_lastNumTexturesTotal = numTexturesTotal;
			DrawText(xAdj + 300.0f, yAdj + 170.0f, texturesAtRuntime ? colorListSelected : color, "#Tex: %d / #TexTotal: %d%s",
			         GTextureXRenderBase::GetNumTextures(), s_lastNumTexturesTotal, texturesAtRuntime ? " !Textures created at runtime!" : "");

		#if defined(USE_GFX_VIDEO)
			GPtr<GFxVideoBase> pVideo = pLoader ? pLoader->GetVideo() : 0;
			DrawText(xAdj + 10.0f, yAdj + 182.0f, color, "VideoHeap Used       : %6dk", InKB(pVideo && pVideo->GetHeap() ? pVideo->GetHeap()->GetTotalUsedSpace() : 0));
		#else
			DrawText(xAdj + 10.0f, yAdj + 182.0f, color, "Video support disabled");
		#endif
		}

		// sort lexicographically if requested
		if (CFlashInfoInput::Get()->ShouldSort())
		{
			bool needSorting(true);
			while (needSorting)
			{
				needSorting = false;
				PlayerListNodeType* pNode = GetListRoot().m_pNext;
				PlayerListNodeType* pNext = pNode->m_pNext;
				while (pNext != &GetListRoot())
				{
					const char* pFileNameCur = pNode->m_pHandle->m_filePath->c_str();
					const char* pFileNameNext = pNext->m_pHandle->m_filePath->c_str();
					if (SFlashProfilerData::ShowStrippedPath())
					{
						pFileNameCur = PathUtil::GetFile(pFileNameCur);
						pFileNameNext = PathUtil::GetFile(pFileNameNext);
					}

					if (stricmp(pFileNameCur, pFileNameNext) > 0)
					{
						needSorting = true;

						PlayerListNodeType* pNodePrev = pNode->m_pPrev;
						PlayerListNodeType* pNextNext = pNext->m_pNext;

						pNodePrev->m_pNext = pNext;
						pNextNext->m_pPrev = pNode;

						pNode->m_pPrev = pNext;
						pNode->m_pNext = pNextNext;

						pNext->m_pPrev = pNodePrev;
						pNext->m_pNext = pNode;
					}
					else
						pNode = pNode->m_pNext;

					pNext = pNode->m_pNext;
				}
			}
		}

		const bool histPrevWriteProtected = SFlashProfilerData::HistogramWriteProtected();

		// toggle write protection to pause profiling
		// (should be done before display & gathering of profile data in next frame)
		if (CFlashInfoInput::Get()->TogglePause())
		{
			SFlashProfilerData::ToggleHistogramWriteProtection();
			s_flashPeakHistory.ToggleWriteProtection();
		}

		if (CFlashInfoInput::Get()->ToggleFreeze())
			SFlashProfilerData::ToggleDisplayFrozen();

		if (CFlashInfoInput::Get()->ToggleShowStrippedPath())
			SFlashProfilerData::ToggleShowStrippedPath();

		// ypos = 190.0f; placeholder for number of (active) flash files
		unsigned int numFlashFiles(0), numActiveFlashFiles(0);

		// ypos = 208.0f; placeholder for overall cost of flash processing / rendering
		// display flash instance profiling results
		static PlayerListNodeType* s_pLastSelectedLinkNode = GetListRoot().m_pNext;
		bool lastSelectedLinkNodeLost(true);

		const float deltaFrameTime(gEnv->pTimer->GetFrameTime());
		const float blendFactor(expf(-deltaFrameTime / 0.35f));
		float ypos(238.0f);
		PlayerListNodeType* pCurNode = GetListRoot().m_pNext;
		while (pCurNode != &GetListRoot())
		{
			if (s_pLastSelectedLinkNode == pCurNode)
				lastSelectedLinkNodeLost = false;

			CFlashPlayer* pFlashPlayer(pCurNode->m_pHandle);
			SFlashProfilerData* pFlashProfilerData(pFlashPlayer ? pFlashPlayer->m_pProfilerData : 0);

			float funcCostAvg[eNumFncIDs], funcCostMin[eNumFncIDs], funcCostMax[eNumFncIDs], funcCostCur[eNumFncIDs];
			float totalCostAvg(0), totalCostMin(0), totalCostMax(0), totalCostCur(0);
			if (pFlashProfilerData)
			{
				for (unsigned int i(0); i < eNumFncIDs; ++i)
				{
					funcCostAvg[i] = pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetAvg();
					funcCostMin[i] = pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetMin();
					funcCostMax[i] = pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetMax();
					funcCostCur[i] = pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetCur();
				}

				totalCostAvg = pFlashProfilerData->GetTotalTimeHistoAllFuncs().GetAvg();
				totalCostMin = pFlashProfilerData->GetTotalTimeHistoAllFuncs().GetMin();
				totalCostMax = pFlashProfilerData->GetTotalTimeHistoAllFuncs().GetMax();
				totalCostCur = pFlashProfilerData->GetTotalTimeHistoAllFuncs().GetCur();
			}
			else
			{
				for (unsigned int i(0); i < eNumFncIDs; ++i)
				{
					funcCostAvg[i] = 0;
					funcCostMin[i] = 0;
					funcCostMax[i] = 0;
					funcCostCur[i] = 0;
				}
			}

			const char* pVisExpChar(" ");
			if (pFlashProfilerData)
				pVisExpChar = pFlashProfilerData->IsViewExpanded() ? "-" : "+";

			const char* pInstanceInfo("");
			if (pFlashPlayer && (!pFlashPlayer->m_pMovieDef || !pFlashPlayer->m_pMovieView))
				pInstanceInfo = " !instantiated but Load() failed!";

			const bool disabled = pFlashProfilerData && pFlashProfilerData->PreventFunctionExectution();
			const char* pDisabled = !disabled ? "" : "[disabled] ";

			const bool selected(s_pLastSelectedLinkNode == pCurNode);
			const bool inactive(!pFlashProfilerData || totalCostAvg < 1e-3f);

			const float* col(0);
			if (inactive)
				col = (!selected) ? colorListInactive : colorListSelectedInactive;
			else
				col = (!selected) ? colorList : colorListSelected;

			if (pFlashProfilerData)
			{
				threadID idx = 0;
				pRenderer->EF_Query(EFQ_MainThreadList, idx);
				pFlashProfilerData->UpdateDrawStats(idx, !histPrevWriteProtected);
			}

			DrawText(xAdj + 10.0f, yAdj + ypos, col, "%.2f", totalCostAvg);
			if (pFlashPlayer)
			{
				const char* pFileName = pFlashPlayer->m_filePath->c_str();
				if (SFlashProfilerData::ShowStrippedPath())
					pFileName = PathUtil::GetFile(pFileName);

				if (ms_sys_flash_info != 2)
				{
					size_t memMovieView = 0;
					GFxMovieView* pMovieView = pFlashPlayer->m_pMovieView.GetPtr();
					if (pMovieView)
					{
						GMemoryHeap* pHeap = pMovieView->GetHeap();
						memMovieView += pHeap ? pHeap->GetTotalUsedSpace() : 0;
					}

					size_t memMovieDef = 0;
					GFxMovieDef* pMovieDef = pFlashPlayer->m_pMovieDef.GetPtr();
					if (pMovieDef)
					{
						GMemoryHeap* pHeapMovieData = pMovieDef->GetLoadDataHeap(); // "MovieData"
						memMovieDef += pHeapMovieData ? pHeapMovieData->GetTotalUsedSpace() : 0;

						//GMemoryHeap* pHeapMovieImageData = pMovieDef->GetImageHeap(); // "_Images"; child of "MovieData" and therefor accounted for already
						//memMovieDef += pHeapMovieImageData ? pHeapMovieImageData->GetTotalUsedSpace() : 0;

						//GMemoryHeap* pHeapMovieDef = pMovieDef->GetBindDataHeap(); // used to be "MovieDef", now global heap and therefore not to be counted here
						//memMovieDef += pHeapMovieDef ? pHeapMovieDef->GetTotalUsedSpace() : 0;
					}

					const char* pLocklessDisparityMsg = "";
					const char* pLocklessDisparityMsgCol = "";
					if (!disabled && !inactive && pFlashProfilerData && pFlashProfilerData->GetLocklessDisparity().GetMax() > 0)
					{
						pLocklessDisparityMsg = "!lockless record/playback disparity!$O ";
						pLocklessDisparityMsgCol = ((curFrameID / 10) & 1) != 0 ? "$8" : "$4";
					}

					DrawText(xAdj + 50.0f, yAdj + ypos, col, " %s[ar: %d|def: 0x%p, %4dk|view: 0x%p, %4dk|cb: %4dk|vis: %d] %s%s%s%s%s",
					         pVisExpChar, pFlashPlayer->m_memArenaID, pFlashPlayer->m_pMovieDef.GetPtr(), InKB(memMovieDef), pFlashPlayer->m_pMovieView.GetPtr(),
					         InKB(memMovieView), InKB(pFlashPlayer->GetCommandBufferSize()), !inactive && pFlashPlayer->GetVisible() ? 1 : 0,
					         pLocklessDisparityMsgCol, pLocklessDisparityMsg, pDisabled, pFileName, pInstanceInfo);
				}
				else
				{
					unsigned int dcCur = 0, dcAvg = 0, triCur = 0, triAvg = 0;
					if (pFlashProfilerData)
					{
						const SFlashProfilerData::DrawCallHistogram& dc = pFlashProfilerData->GetDrawCallHisto();
						dcCur = dc.GetCur();
						dcAvg = dc.GetAvg();
						const SFlashProfilerData::DrawCallHistogram& tri = pFlashProfilerData->GetTriCountHisto();
						triCur = tri.GetCur();
						triAvg = tri.GetAvg();
					}
					DrawText(xAdj + 50.0f, yAdj + ypos, col, "  [draw calls: cur %4d, avg %4d|tris: cur %5d, avg %5d] %s%s", dcCur, dcAvg, triCur, triAvg, pDisabled, pFileName);
				}
			}
			ypos += 12.0f;

			if (ms_sys_flash_info == 1 && pFlashProfilerData && pFlashProfilerData->IsViewExpanded())
			{
				float colChannelMult(!inactive ? 1 : inactiveColChannelMult);
				float colorDyn[4] = { 0, 0, 0, 0 };
				{
					float newColorValue(totalCostCur * (1 - blendFactor) + pFlashProfilerData->GetColorValue(eNumFncIDs) * blendFactor);
					float variance(CalculateVariance(totalCostCur, totalCostAvg));
					float newColorValueVariance(variance * (1 - blendFactor) + pFlashProfilerData->GetColorValueVariance(eNumFncIDs) * blendFactor);

					pFlashProfilerData->SetColorValue(eNumFncIDs, newColorValue);
					pFlashProfilerData->SetColorValueVariance(eNumFncIDs, newColorValueVariance);

					CalculateColor(newColorValue, newColorValueVariance, colorDyn);
					colorDyn[0] *= colChannelMult;
					colorDyn[1] *= colChannelMult;
					colorDyn[2] *= colChannelMult;

					ypos += 4.0f;
					DrawText(xAdj + 60.0f, yAdj + ypos, colorDyn, "Total:");
					DrawText(xAdj + 170.0f, yAdj + ypos, colorDyn, "avg %.2f", totalCostAvg);
					DrawText(xAdj + 245.0f, yAdj + ypos, colorDyn, "/ min %.2f", totalCostMin);
					DrawText(xAdj + 332.0f, yAdj + ypos, colorDyn, "/ max %.2f", totalCostMax);
					ypos += 20.0f;
				}

				if (ms_sys_flash_info_histo_scale < 1e-2f)
					ms_sys_flash_info_histo_scale = 1e-2f;
				float histoScale(ms_sys_flash_info_histo_scale * 50.0f);

				for (unsigned int i(0); i < eNumFncIDs; ++i)
				{
					float newColorValue(funcCostCur[i] * (1 - blendFactor) + pFlashProfilerData->GetColorValue((EFlashFunctionID)i) * blendFactor);
					float variance(CalculateVariance(funcCostCur[i], funcCostAvg[i]));
					float newColorValueVariance(variance * (1 - blendFactor) + pFlashProfilerData->GetColorValueVariance((EFlashFunctionID)i) * blendFactor);

					pFlashProfilerData->SetColorValue((EFlashFunctionID)i, newColorValue);
					pFlashProfilerData->SetColorValueVariance((EFlashFunctionID)i, newColorValueVariance);

					CalculateColor(newColorValue, newColorValueVariance, colorDyn);
					colorDyn[0] *= colChannelMult;
					colorDyn[1] *= colChannelMult;
					colorDyn[2] *= colChannelMult;

					unsigned int numAvgCalls(pFlashProfilerData->GetTotalCallsHisto((EFlashFunctionID)i).GetAvg());
					DrawText(xAdj + 25.0f, yAdj + ypos, colorDyn, "%d/", numAvgCalls);
					DrawText(xAdj + 60.0f, yAdj + ypos, colorDyn, "%s()", GetFlashFunctionName((EFlashFunctionID)i));
					DrawText(xAdj + 170.0f, yAdj + ypos, colorDyn, "avg %.2f", funcCostAvg[i]);
					DrawText(xAdj + 245.0f, yAdj + ypos, colorDyn, "/ min %.2f", funcCostMin[i]);
					DrawText(xAdj + 332.0f, yAdj + ypos, colorDyn, "/ max %.2f", funcCostMax[i]);

					if (!isMTRenderer)
					{
						unsigned char histogram[SFlashProfilerData::HistogramSize];
						for (unsigned int j(0); j < SFlashProfilerData::HistogramSize; ++j)
							histogram[j] = 255 - (unsigned int) clamp_tpl(pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetAt(j) * histoScale, 0.0f, 255.0f);

						ColorF histogramCol(0, 1, 0, 1);
						pRenderer->Graph(histogram, (int) (xAdj + 440), (int) (yAdj + ypos), SFlashProfilerData::HistogramSize, 14, pFlashProfilerData->GetTotalTimeHisto((EFlashFunctionID)i).GetCurIdx(), 2, 0, histogramCol, 1);
					}

					ypos += 16.0f;
				}
			}
		#if defined(ENABLE_DYNTEXSRC_PROFILING)
			else if (ms_sys_flash_info == 6 && pFlashProfilerData && pFlashProfilerData->IsViewExpanded())
			{
				ypos += 4.0f;
				const IDynTextureSource* pSrc = pFlashPlayer->m_pDynTexSrc;
				DrawText(xAdj + 70.0f, yAdj + ypos, col, pSrc ? pSrc->GetProfileInfo().c_str() : "not a flash material");
				ypos += 16.0f;
			}
		#endif

			if (pFlashProfilerData)
				pFlashProfilerData->Advance();

			++numFlashFiles;
			if (!inactive)
				++numActiveFlashFiles;

			pCurNode = pCurNode->m_pNext;
		}

		// fill placeholders
		DrawText(xAdj + 10.0f, yAdj + 202.0f, color, "#FlashObj : %d", numFlashFiles);
		DrawText(xAdj + 130.0f, yAdj + 202.0f, colorList, "/ #Active : %d", numActiveFlashFiles);
		DrawText(xAdj + 250.0f, yAdj + 202.0f, colorListInactive, "/ #Inactive : %d", numFlashFiles - numActiveFlashFiles);
		#if defined(USE_GFX_VIDEO)
		DrawText(xAdj + 390.0f, yAdj + 202.0f, color, "/ #Videos : %d", GFxVideoWrapper::GetNumVideoInstances());
		#endif

		DrawText(xAdj + 10.0f, yAdj + 220.0f, color, "Total:");
		DrawText(xAdj + 70.0f, yAdj + 220.0f, color, "avg %.2f", SFlashProfilerData::GetTotalTimeHistoAllInsts().GetAvg());
		DrawText(xAdj + 155.0f, yAdj + 220.0f, color, "/ min %.2f", SFlashProfilerData::GetTotalTimeHistoAllInsts().GetMin());
		DrawText(xAdj + 252.0f, yAdj + 220.0f, color, "/ max %.2f", SFlashProfilerData::GetTotalTimeHistoAllInsts().GetMax());

		SFlashProfilerData::AdvanceAllInsts();
		{
			float xpos = xAdj + 350.0f;
			if (SFlashProfilerData::HistogramWriteProtected())
			{
				DrawText(xpos, yAdj + 208.0f, colorList, "[paused]");
				xpos += 75.0f;
			}

			if (SFlashProfilerData::DisplayFrozen())
				DrawText(xpos, yAdj + 208.0f, colorListSelected, "[display frozen]");
		}
		// display flash peak history
		ypos += 4.0f;
		float displayTime(gEnv->pTimer->GetAsyncCurTime());
		DrawText(xAdj + 10.0f, yAdj + ypos, color, "Latest flash peaks (tolerance = %.2f ms)...", ms_sys_flash_info_peak_tolerance);
		ypos += 16.0f;

		s_flashPeakHistory.UpdateHistory(displayTime);
		const float maxFlashPeakDisplayTime(s_flashPeakHistory.GetTimePeakExpires());

		const CFlashPeakHistory::FlashPeakHistory& peakHistory(s_flashPeakHistory.GetHistory());
		CFlashPeakHistory::FlashPeakHistory::const_iterator it(peakHistory.begin());
		CFlashPeakHistory::FlashPeakHistory::const_iterator itEnd(peakHistory.end());
		for (; it != itEnd; )
		{
			const SFlashPeak& peak(*it);
			float colorDyn[4] = { 0, 0, 0, 0 };
			CalculateColor(clamp_tpl(1.0f - (displayTime - peak.m_timeRecorded) / maxFlashPeakDisplayTime, 0.0f, 1.0f), colorDyn);
			DrawTextNoFormat(xAdj + 10.0f, yAdj + ypos, colorDyn, peak.m_displayInfo.c_str());
			ypos += 12.0f;
			++it;
		}

		// update peak history function exclude mask
		CFlashFunctionProfiler::ResetPeakFuncExcludeMask();
		const char* pExludeMaskStr(CV_sys_flash_info_peak_exclude ? CV_sys_flash_info_peak_exclude->GetString() : 0);
		if (pExludeMaskStr)
		{
			for (unsigned int i(0); i < eNumFncIDs; ++i)
			{
				EFlashFunctionID funcID((EFlashFunctionID) i);
				const char* pFlashFuncName(GetFlashFunctionName(funcID));
				if (strstr(pExludeMaskStr, pFlashFuncName))
					CFlashFunctionProfiler::AddToPeakFuncExcludeMask(funcID);
			}
		}

		// Temp render target info
		if (pFlashRenderer)
		{
			const std::vector<ITexture*>& tempRenderTargets = pFlashRenderer->GetTempRenderTargets();
			int nTempRenderTargets = tempRenderTargets.size();

			if (nTempRenderTargets > 0)
			{
				ypos += 8.f;

				DrawText(xAdj + 10.0f, yAdj + ypos, color, "Temp render targets:");
				ypos += 12.f;

				for (int i = 0; i < nTempRenderTargets; i++)
				{
					ITexture* pTex = tempRenderTargets[i];

					UInt rtWidth = pTex->GetWidth();
					UInt rtHeight = pTex->GetHeight();

					DrawText(xAdj + 20.0f, yAdj + ypos, color, "RT(%s), width: %d height: %d", pTex->GetName(), rtWidth, rtHeight);

					ypos += 12.f;
				}
			}
		}

		// allow user navigation for detailed profiling statistics
		if (lastSelectedLinkNodeLost)
			s_pLastSelectedLinkNode = GetListRoot().m_pNext;

		if (CFlashInfoInput::Get()->ShouldMoveUp())
		{
			s_pLastSelectedLinkNode = s_pLastSelectedLinkNode->m_pPrev;
			if (s_pLastSelectedLinkNode == &GetListRoot())
				s_pLastSelectedLinkNode = s_pLastSelectedLinkNode->m_pPrev;
		}

		if (CFlashInfoInput::Get()->ShouldMoveDown())
		{
			s_pLastSelectedLinkNode = s_pLastSelectedLinkNode->m_pNext;
			if (s_pLastSelectedLinkNode == &GetListRoot())
				s_pLastSelectedLinkNode = s_pLastSelectedLinkNode->m_pNext;
		}

		if (CFlashInfoInput::Get()->ShouldScrollUp())
		{
			PlayerListNodeType* pNode = &GetListRoot();
			PlayerListNodeType* pNext = GetListRoot().m_pNext;

			PlayerListNodeType* pNodePrev = pNode->m_pPrev;
			PlayerListNodeType* pNextNext = pNext->m_pNext;

			pNodePrev->m_pNext = pNext;
			pNextNext->m_pPrev = pNode;

			pNode->m_pPrev = pNext;
			pNode->m_pNext = pNextNext;

			pNext->m_pPrev = pNodePrev;
			pNext->m_pNext = pNode;
		}

		if (CFlashInfoInput::Get()->ShouldScrollDown())
		{
			PlayerListNodeType* pNode = &GetListRoot();
			PlayerListNodeType* pPrev = GetListRoot().m_pPrev;

			PlayerListNodeType* pNodeNext = pNode->m_pNext;
			PlayerListNodeType* pPrevPrev = pPrev->m_pPrev;

			pNodeNext->m_pPrev = pPrev;
			pPrevPrev->m_pNext = pNode;

			pNode->m_pNext = pPrev;
			pNode->m_pPrev = pPrevPrev;

			pPrev->m_pNext = pNodeNext;
			pPrev->m_pPrev = pNode;
		}

		if (CFlashInfoInput::Get()->OpenRequested())
		{
			if (s_pLastSelectedLinkNode != &GetListRoot())
			{
				CFlashPlayer* pFlashPlayer = s_pLastSelectedLinkNode->m_pHandle;
				SFlashProfilerData* pFlashProfilerData = pFlashPlayer ? pFlashPlayer->m_pProfilerData : 0;
				if (pFlashProfilerData)
					pFlashProfilerData->SetViewExpanded(true);
			}
		}

		if (CFlashInfoInput::Get()->CloseRequested())
		{
			if (s_pLastSelectedLinkNode != &GetListRoot())
			{
				CFlashPlayer* pFlashPlayer = s_pLastSelectedLinkNode->m_pHandle;
				SFlashProfilerData* pFlashProfilerData = pFlashPlayer ? pFlashPlayer->m_pProfilerData : 0;
				if (pFlashProfilerData)
					pFlashProfilerData->SetViewExpanded(false);
			}
		}

		if (CFlashInfoInput::Get()->ToggleDisable())
		{
			if (s_pLastSelectedLinkNode != &GetListRoot())
			{
				CFlashPlayer* pFlashPlayer = s_pLastSelectedLinkNode->m_pHandle;
				SFlashProfilerData* pFlashProfilerData = pFlashPlayer ? pFlashPlayer->m_pProfilerData : 0;
				if (pFlashProfilerData)
					pFlashProfilerData->TogglePreventFunctionExecution();
			}
		}
	}
	else
	{
		if (pFlashRenderer)
			pFlashRenderer->GetRenderStats(0, true);

		s_lastNumTexturesTotal = GTextureXRenderBase::GetNumTexturesTotal();

		CFlashInfoInput::Reset();

		SFlashProfilerData::ResetDisplayFrozen();

		#if defined(USE_GFX_VIDEO)
		{
			const int numVideoInstances = GFxVideoWrapper::GetNumVideoInstances();
			if (numVideoInstances)
			{
				static ICVar* pCVarDisplInfo = gEnv->pConsole->GetCVar("r_DisplayInfo");
				if (pCVarDisplInfo && pCVarDisplInfo->GetIVal())
				{
					static const float color[4] = { 1, 1, 1, 1 };
					const char* videoCol = ((curFrameID / 10) & 1) != 0 ? "$8" : "$4";
					DrawText(0, 0, color, "%s%d video(s) active$O", videoCol, numVideoInstances);
				}
			}
		}
		#endif
	}

	if (ms_sys_flash_reset_mesh_cache)
	{
		CSharedFlashPlayerResources::GetAccess().ResetMeshCache();
		ms_sys_flash_reset_mesh_cache = 0;
	}

	#else // #if defined(ENABLE_FLASH_INFO)

		#if defined(USE_PERFHUD) || defined(ENABLE_STATOSCOPE)

	// Both Statoscope and PerfHUD need the flash render stats reset should ENABLE_FLASH_INFO be disabled
	IScaleformRecording* pFlashRenderer = CSharedFlashPlayerResources::GetAccess().GetRenderer(true);
	if (pFlashRenderer)
		pFlashRenderer->GetRenderStats(0, true);

		#endif

	#endif // #if defined(ENABLE_FLASH_INFO)

	#if defined(ENABLE_LW_PROFILERS)
	CFlashFunctionProfilerLight::Update();
	#endif
}

void CFlashPlayer::DumpAndFixLeaks()
{
	#if !defined(RELEASE)
	ILog* pLog = gEnv->pLog;
	assert(pLog);
	bool leaksDetected = false;
	{
		CryAutoCriticalSection lock(GetList().GetLock());

		const PlayerListNodeType* pNode = GetListRoot().m_pNext;
		if (pNode != &GetListRoot())
		{
			leaksDetected = true;
			pLog->LogError("Dumping Flash player leaks:");
			for (; pNode != &GetListRoot(); pNode = pNode->m_pNext)
			{
				CFlashPlayer* pPlayer = pNode->m_pHandle;
				const char* pFilePath = pPlayer->m_filePath->c_str();
				pLog->Log("  ptr = 0x%p [%s]", pPlayer, pFilePath ? pFilePath : "#NULL#");
			}
		}
	}
	{
		CryAutoCriticalSection lock(CFlashVariableObject::GetList().GetLock());

		const CFlashVariableObject::VariableListNodeType* pNode = CFlashVariableObject::GetListRoot().m_pNext;
		if (pNode != &CFlashVariableObject::GetListRoot())
		{
			leaksDetected = true;
			pLog->LogError("Dumping Flash variable object leaks:");
			for (; pNode != &CFlashVariableObject::GetListRoot(); pNode = pNode->m_pNext)
			{
				CFlashVariableObject* pVar = pNode->m_pHandle;
				const stringConstPtr& refFilePath = pVar->GetRefFilePath();
				const char* pRefFilePath = refFilePath->c_str();
				pLog->Log("  ptr = 0x%p, ref = 0x%p [%s]", pVar, refFilePath.get(), pRefFilePath ? pRefFilePath : "#NULL#");
			}
		}
	}
	{
		CryAutoCriticalSection lock(CFlashPlayerBootStrapper::GetList().GetLock());

		const CFlashPlayerBootStrapper::BootStrapperListNodeType* pNode = CFlashPlayerBootStrapper::GetListRoot().m_pNext;
		if (pNode != &CFlashPlayerBootStrapper::GetListRoot())
		{
			leaksDetected = true;
			pLog->LogError("Dumping Flash boot strapper leaks:");
			for (; pNode != &CFlashPlayerBootStrapper::GetListRoot(); pNode = pNode->m_pNext)
			{
				CFlashPlayerBootStrapper* pBootStrapper = pNode->m_pHandle;
				const char* pFilePath = pBootStrapper->GetFilePath();
				pLog->Log("  ptr = 0x%p, def = 0x%p [%s]", pBootStrapper, pBootStrapper->GetMovieDef(), pFilePath ? pFilePath : "#NULL#");
			}
		}
	}
	assert(!leaksDetected);
	if (leaksDetected)
	{
		pLog->LogWarning("Fixing Flash leaks to allow safe shutdown of memory system. "
		                 "Accessing those leaked references after this point will result in crashes. Fix the leaks!");
	#else
	{
	#endif
		{
			CryAutoCriticalSection lock(CFlashVariableObject::GetList().GetLock());
			while (CFlashVariableObject::GetListRoot().m_pNext != &CFlashVariableObject::GetListRoot())
			{
				CFlashVariableObject::GetListRoot().m_pNext->m_pHandle->Release();
			}
		}
		{
			CryAutoCriticalSection lock(CFlashPlayerBootStrapper::GetList().GetLock());
			while (CFlashPlayerBootStrapper::GetListRoot().m_pNext != &CFlashPlayerBootStrapper::GetListRoot())
			{
				CFlashPlayerBootStrapper::GetListRoot().m_pNext->m_pHandle->Release();
			}
		}
	}
}

bool CFlashPlayer::AllowMeshCacheReset()
{
	return ms_sys_flash_allow_mesh_cache_reset != 0;
}

IFlashPlayerBootStrapper* CFlashPlayer::CreateBootstrapper()
{
	return new CFlashPlayerBootStrapper();
}

void MeshCacheResetThread::ThreadEntry()
{
	while (!m_cancelRequestSent)
	{
		m_awakeThread.Wait();
		if (!m_cancelRequestSent)
		{
			s_displaySync.Lock();
			if (CFlashPlayer::AllowMeshCacheReset())
			{
				GFxLoader2* pLoader = CSharedFlashPlayerResources::GetAccess().GetLoader(true);
				if (pLoader)
					pLoader->GetMeshCacheManager()->ClearCache();
			}
			s_displaySync.Unlock();
		}
		m_awakeThread.Reset();
	}
}

void CFlashPlayer::SetFlashLoadMovieHandler(IFlashLoadMovieHandler* pHandler)
{
	ms_pLoadMovieHandler = pHandler;
}

IFlashLoadMovieHandler* CFlashPlayer::GetFlashLoadMovieHandler()
{
	return ms_pLoadMovieHandler;
}

unsigned int CFlashPlayer::GetLogOptions()
{
	return ms_sys_flash_log_options;
}

void CFlashPlayer::GetFlashProfileResults(float& accumTime)
{
	#if defined(INCLUDE_SCALEFORM_SDK) && defined(ENABLE_LW_PROFILERS)
		#if defined(ENABLE_FLASH_INFO)
	const bool calcCost = ms_sys_flash_info == 0; // don't let lightweight profiler measure cost of full profiler when active (sys_flash_info 1)
		#else
	const bool calcCost = true;
		#endif
	accumTime = calcCost ? CFlashFunctionProfilerLight::GetAccumulatedTime() : -1.0f;
	#else
	accumTime = -1.0f;
	#endif
}

size_t CFlashPlayer::GetStaticPoolSize()
{
	return (size_t)ms_sys_flash_static_pool_size * 1024;
}

size_t CFlashPlayer::GetAddressSpaceSize()
{
	return (size_t)ms_sys_flash_address_space_kb << 10;
}

#endif                                          // #ifdef INCLUDE_SCALEFORM_SDK
