// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Stroboscope.h
//  Version:     v1.00
//  Created:     17/6/2012 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __Stroboscope_H__
#define __Stroboscope_H__

#if defined(ENABLE_PROFILING_CODE)

	#include "ThreadInfo.h"
	#include <CryThreading/IThreadManager.h>

	#define MAX_CALLSTACK_DEPTH 256

struct SStrobosopeSamplingData
{
	struct SCallstackSampling
	{
		// Note: Stack not initialized on purpose, accessed items guarded by Depth
		// cppcheck-suppress uninitMemberVar
		SCallstackSampling() : Depth(0) {};
		bool operator<(const SCallstackSampling& other) const;

		void* Stack[MAX_CALLSTACK_DEPTH];
		int   Depth;
	};
	typedef std::map<SCallstackSampling, float> TCallstacks;
	typedef std::map<int, TCallstacks>          TFrameCallstacks;
	typedef std::map<int, float>                TFrameTime;

	struct SThreadResult
	{

		TFrameCallstacks Callstacks;
		int              Samples;
		string           Name;
	};
	typedef std::map<uint32, SThreadResult> TThreadResult;

	bool          Valid;
	float         Duration;
	TThreadResult Threads;
	TFrameTime    FrameTime;
	int           StartFrame;
	int           EndFrame;

	void          Clear()
	{
		Valid = false;
		Duration = 0;
		StartFrame = 0;
		EndFrame = 0;
		Threads.clear();
		FrameTime.clear();
	}
};

struct SStrobosopeResult
{
	struct SSymbolInfo
	{
		string Module;
		string Procname;
		string File;
		int    Line;
		void*  BaseAddr;
	};
	typedef std::map<int, SSymbolInfo> TSymbolInfo;

	struct SCallstackInfo
	{
		float    Spend;
		typedef std::vector<int> TSymbols;
		TSymbols Symbols;
		uint32   ThreadId;
		int      FrameId;
	};
	typedef std::vector<SCallstackInfo> TCallstackInfo;

	struct SThreadInfo
	{
		uint32 Id;
		string Name;
		int    Samples;
		float  Counts;
	};
	typedef std::vector<SThreadInfo> TThreadInfo;
	typedef std::map<int, float>     TFrameTime;

	SStrobosopeResult()
		: Valid(false)
		, Duration(0.0f)
		, Samples(0)
		, TotalCounts(0.0f)
		, StartFrame(0)
		, EndFrame(0)
	{}

	bool           Valid;
	float          Duration;
	string         File;
	string         Date;
	int            Samples;
	float          TotalCounts;
	int            StartFrame;
	int            EndFrame;
	TSymbolInfo    SymbolInfo;
	TCallstackInfo CallstackInfo;
	TThreadInfo    ThreadInfo;
	TFrameTime     FrameTime;

	void           Clear()
	{
		Valid = false;
		Duration = 0;
		File = "";
		Date = "";
		Samples = 0;
		TotalCounts = 0;
		StartFrame = 0;
		EndFrame = 0;
		FrameTime.clear();
		SymbolInfo.clear();
		CallstackInfo.clear();
		ThreadInfo.clear();
	}
};

class CStroboscope : public IThread
{
public:
	static CStroboscope*     GetInst() { static CStroboscope inst; return &inst; }

	void                     RegisterCommands();

	void                     StartProfiling(float deltaStart = 0, float duration = -1, int throttle = 100, const SThreadInfo::TThreadIds threadIds = SThreadInfo::TThreadIds());
	void                     StopProfiling();
	const SStrobosopeResult& GetResult() { UpdateResult(); return m_result; }

	// Start accepting work on thread
	virtual void ThreadEntry();

	static void  StartProfilerCmd(IConsoleCmdArgs* pArgs);
	static void  StopProfilerCmd(IConsoleCmdArgs* pArgs);
	static void  DumpResultsCmd(IConsoleCmdArgs* pArgs);
private:
	CStroboscope();
	~CStroboscope();

	void ProfileThreads();
	bool SampleThreads(const SThreadInfo::TThreads& threads, float delta, int frameId);
	bool SampleThread(const SThreadInfo::SThreadHandle& thread, float delta, int frameId);

	void UpdateResult();
	void DumpOutputResult();

private:
	float                   m_startTime;
	float                   m_endTime;
	int                     m_throttle;
	SThreadInfo::TThreadIds m_threadIds;
	volatile LONG           m_started;

	SStrobosopeSamplingData m_sampling;
	SStrobosopeResult       m_result;

	bool                    m_run;
};

#endif //#if defined(ENABLE_PROFILING_CODE)

#endif //#ifndef __Stroboscope_H__
