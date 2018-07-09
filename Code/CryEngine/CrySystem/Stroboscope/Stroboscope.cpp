// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Stroboscope.cpp
//  Version:     v1.00
//  Created:     17/6/2012 by Paul Reindell.
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"

#if defined(ENABLE_PROFILING_CODE)

	#include "Stroboscope.h"
	#include "IDebugCallStack.h"

////////////////////////////////////////////////////////////////////////////
bool SStrobosopeSamplingData::SCallstackSampling::operator<(const SCallstackSampling& other) const
{
	if (Depth == other.Depth)
	{
		for (int i = 0; i < Depth; ++i)
		{
			if (Stack[i] < other.Stack[i])
				return true;
			else if (Stack[i] > other.Stack[i])
				return false;
		}
	}
	return Depth < other.Depth;
}

////////////////////////////////////////////////////////////////////////////
CStroboscope::CStroboscope()
	: m_started(0)
	, m_startTime(0)
	, m_endTime(-1)
	, m_throttle(100)
	, m_run(false)
{
	m_result.Clear();
	m_sampling.Clear();
}

////////////////////////////////////////////////////////////////////////////
CStroboscope::~CStroboscope()
{
	StopProfiling();
}

////////////////////////////////////////////////////////////////////////////
void CStroboscope::RegisterCommands()
{
	REGISTER_COMMAND("sys_StroboscopeStart", &StartProfilerCmd, 0, "Starts the stroboscope profiler.\n"
	                                                               "Usage:\n"
	                                                               "sys_StroboscopeStart [Duration] [StartDelay] [Throttle] [ThreadId...]");

	REGISTER_COMMAND("sys_StroboscopeStop", &StopProfilerCmd, 0, "Stops the stroboscope profiler.");
	REGISTER_COMMAND("sys_StroboscopeDumpResults", &DumpResultsCmd, 0, "Dumps the results onto disk.");
}

////////////////////////////////////////////////////////////////////////////
void CStroboscope::StartProfiling(float deltaStart /* = 0*/, float duration /* = -1*/, int throttle /* = 100*/, const SThreadInfo::TThreadIds threadIds /* = SThreadInfo::TThreadIds()*/)
{
	if (CryInterlockedCompareExchange(&m_started, 1, 0) == 0)
	{
		m_startTime = gEnv->pTimer->GetAsyncCurTime() + deltaStart;
		m_endTime = duration > 0 ? m_startTime + duration : -1;
		m_throttle = throttle;
		Limit(m_throttle, 1, 100);
		m_threadIds = threadIds;
		m_result.Clear();
		m_sampling.Clear();
		m_run = true;
		if (!gEnv->pThreadManager->SpawnThread(this, "Stroboscope"))
		{
			CryFatalError("Error spawning \"Stroboscope\" thread.");
		}
	}
}

////////////////////////////////////////////////////////////////////////////
void CStroboscope::StopProfiling()
{
	m_run = false;
}

////////////////////////////////////////////////////////////////////////////
void CStroboscope::ThreadEntry()
{
	while (gEnv->pTimer->GetAsyncCurTime() < m_startTime)
		CrySleep(10);

	gEnv->pLog->LogAlways("[Stroboscope] Profiling started!");

	ProfileThreads();

	gEnv->pLog->LogAlways("[Stroboscope] Profiling done!");

	CryInterlockedCompareExchange(&m_started, 0, 1);
}

////////////////////////////////////////////////////////////////////////////
void CStroboscope::ProfileThreads()
{
	if (!IDebugCallStack::instance())
		return;

	IDebugCallStack::instance()->InitSymbols();

	SThreadInfo::TThreads threads;
	SThreadInfo::OpenThreadHandles(threads, m_threadIds);
	SThreadInfo::TThreadInfo threadInfo;
	SThreadInfo::GetCurrentThreads(threadInfo);
	for (SThreadInfo::TThreads::const_iterator it = threads.begin(); it != threads.end(); ++it)
	{
		m_sampling.Threads[it->Id].Samples = 0;
		m_sampling.Threads[it->Id].Name = threadInfo[it->Id];
	}

	uint64 freq = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*) &freq);

	int64 curr, prev, start;
	curr = prev = start = CryGetTicks();

	int frameId = -1;
	m_sampling.StartFrame = -1;
	while (m_run)
	{
		CrySleep(100 / m_throttle);
		frameId = gEnv->pRenderer->GetFrameID();
		if (m_sampling.StartFrame == -1)
			m_sampling.StartFrame = frameId;
		std::random_shuffle(threads.begin(), threads.end());
		curr = CryGetTicks();
		if (!SampleThreads(threads, (float)(curr - prev) / (float)freq, frameId) || (m_endTime > 0 && gEnv->pTimer->GetAsyncCurTime() > m_endTime))
			m_run = false;
		prev = curr;
		m_sampling.FrameTime[frameId] = gEnv->pTimer->GetRealFrameTime();
	}
	m_sampling.EndFrame = frameId;
	int64 end = CryGetTicks();
	m_sampling.Duration = (float)(end - start) / (float)freq;
	m_sampling.Valid = true;

	SThreadInfo::CloseThreadHandles(threads);
}

////////////////////////////////////////////////////////////////////////////
bool CStroboscope::SampleThreads(const SThreadInfo::TThreads& threads, float delta, int frameId)
{
	// sample threads
	int activeThreads = 0;
	for (int i = 0, end = threads.size(); i < end; ++i)
	{
		if (SampleThread(threads[i], delta, frameId))
		{
			m_sampling.Threads[threads[i].Id].Samples++;
			activeThreads++;
		}
	}
	return activeThreads > 0;
}

////////////////////////////////////////////////////////////////////////////
bool CStroboscope::SampleThread(const SThreadInfo::SThreadHandle& thread, float delta, int frameId)
{
	#if !CRY_PLATFORM_LINUX && !CRY_PLATFORM_ANDROID && !CRY_PLATFORM_APPLE && !CRY_PLATFORM_ORBIS// No SuspendThread / ResumeThread with pthreads natively
	if (SuspendThread(thread.Handle) != -1)
	{
		SStrobosopeSamplingData::SCallstackSampling callstack;
		callstack.Depth = IDebugCallStack::instance()->CollectCallStack(thread.Handle, callstack.Stack, MAX_CALLSTACK_DEPTH);
		const bool ok = ResumeThread(thread.Handle) != -1;
		assert(ok);

		if (callstack.Depth > 0)
			m_sampling.Threads[thread.Id].Callstacks[frameId][callstack] += delta;

		return true;
	}
	#endif
	return false;
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CStroboscope::UpdateResult()
{
	if (m_result.Valid)
		return;

	m_result.Valid = m_sampling.Valid;
	m_result.Samples = 0;
	m_result.TotalCounts = 0;

	if (m_result.Valid)
	{
		// stats
		time_t rawtime;
		time(&rawtime);
		m_result.File = IDebugCallStack::instance()->GetCurrentFilename();
		m_result.Duration = m_sampling.Duration;
		m_result.Date = asctime(localtime(&rawtime));
		m_result.Date = m_result.Date.replace("\n", "");
		m_result.Date = m_result.Date.replace("\r", "");
		m_result.FrameTime = m_sampling.FrameTime;
		m_result.StartFrame = m_sampling.StartFrame;
		m_result.EndFrame = m_sampling.EndFrame;

		int numsymbols = 0;
		for (SStrobosopeSamplingData::TThreadResult::const_iterator thrd = m_sampling.Threads.begin(); thrd != m_sampling.Threads.end(); ++thrd)
		{
			const SStrobosopeSamplingData::SThreadResult& threadRes = thrd->second;
			uint32 threadId = thrd->first;
			// thread info
			SStrobosopeResult::SThreadInfo threadInfo;
			threadInfo.Id = threadId;
			threadInfo.Name = threadRes.Name;
			threadInfo.Samples = threadRes.Samples;
			threadInfo.Counts = 0;

			// total samples
			m_result.Samples += threadRes.Samples;

			// symbol table
			typedef std::set<void*> TAddresses;
			TAddresses addresses;
			for (SStrobosopeSamplingData::TFrameCallstacks::const_iterator it = threadRes.Callstacks.begin(), end = threadRes.Callstacks.end(); it != end; ++it)
			{
				for (SStrobosopeSamplingData::TCallstacks::const_iterator callIt = it->second.begin(), callEnd = it->second.end(); callIt != callEnd; ++callIt)
				{
					for (int i = 0; i < callIt->first.Depth; ++i)
						addresses.insert(callIt->first.Stack[i]);
				}
			}

			typedef std::map<void*, int> TSymbolTable;
			TSymbolTable symbolidtable;
			for (TAddresses::iterator it = addresses.begin(), end = addresses.end(); it != end; ++it)
			{
				void* addr = *it;
				if (symbolidtable.find(addr) == symbolidtable.end())
				{
					string file, procname;
					int line;
					void* baseAddr;
					bool ok = IDebugCallStack::instance()->GetProcNameForAddr(addr, procname, baseAddr, file, line);
					string module = IDebugCallStack::instance()->GetModuleNameForAddr(addr);

					SStrobosopeResult::SSymbolInfo info;
					info.Procname = procname;
					info.Module = module;
					info.File = file;
					info.Line = line;
					info.BaseAddr = baseAddr;
					m_result.SymbolInfo[numsymbols] = info;
					symbolidtable[addr] = numsymbols++;
				}
			}

			// callstack info
			for (SStrobosopeSamplingData::TFrameCallstacks::const_iterator it = threadRes.Callstacks.begin(), end = threadRes.Callstacks.end(); it != end; ++it)
			{
				for (SStrobosopeSamplingData::TCallstacks::const_iterator callIt = it->second.begin(), callEnd = it->second.end(); callIt != callEnd; ++callIt)
				{
					SStrobosopeResult::SCallstackInfo info;
					info.ThreadId = threadId;
					info.FrameId = it->first;
					info.Spend = callIt->second;
					for (int i = 0; i < callIt->first.Depth; ++i)
						info.Symbols.push_back(symbolidtable[callIt->first.Stack[i]]);
					m_result.CallstackInfo.push_back(info);
				}
			}

			m_result.ThreadInfo.push_back(threadInfo);
		}
	}
}

////////////////////////////////////////////////////////////////////////////
struct SSleepyResultFile
{
	SSleepyResultFile()
	{
		pArchive = gEnv->pCryPak->OpenArchive("stroboscope.sleepy", ICryArchive::FLAGS_RELATIVE_PATHS_ONLY | ICryArchive::FLAGS_CREATE_NEW);
	}

	~SSleepyResultFile()
	{
		if (IsValid())
		{
			for (std::map<string, string>::iterator it = files.begin(); it != files.end(); ++it)
				pArchive->UpdateFile(it->first.c_str(), (void*)it->second.c_str(), it->second.length());
		}
	}

	bool IsValid() const { return pArchive.get() != NULL; }
	void WriteLineToFile(const string& file, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		char tmp[2048];
		cry_vsprintf(tmp, format, args);
		va_end(args);

		files[file] += tmp;
		files[file] += "\r\n";
	}
private:
	_smart_ptr<ICryArchive>  pArchive;
	std::map<string, string> files;
};

void CStroboscope::DumpOutputResult()
{
	const SStrobosopeResult& res = GetResult();
	if (res.Valid)
	{
		SSleepyResultFile file;
		if (!file.IsValid())
		{
			gEnv->pLog->LogError("[Stroboscope] Can't create pak file for profile results!");
			return;
		}
		// Stats.txt
		file.WriteLineToFile("Stats.txt", "Filename: %s", res.File.c_str());
		file.WriteLineToFile("Stats.txt", "Duration: %.6f", res.Duration);
		file.WriteLineToFile("Stats.txt", "Date: %s", res.Date.c_str());
		file.WriteLineToFile("Stats.txt", "Samples: %i", res.Samples);

		// Symbols.txt
		for (SStrobosopeResult::TSymbolInfo::const_iterator it = res.SymbolInfo.begin(), end = res.SymbolInfo.end(); it != end; ++it)
		{
			file.WriteLineToFile("Symbols.txt", "sym%i \"%s\" \"%s\" \"%s\" %i 0x%016llX", it->first, it->second.Module.c_str(), it->second.Procname.c_str(), it->second.File.c_str(), it->second.Line, it->second.BaseAddr);
		}

		// Callstacks.txt
		char tmp[256];
		for (SStrobosopeResult::TCallstackInfo::const_iterator it = res.CallstackInfo.begin(), end = res.CallstackInfo.end(); it != end; ++it)
		{
			string out;
			cry_sprintf(tmp, "%d %u %.6f", it->FrameId, it->ThreadId, it->Spend);
			out = tmp;
			for (SStrobosopeResult::SCallstackInfo::TSymbols::const_iterator sit = it->Symbols.begin(), send = it->Symbols.end(); sit != send; ++sit)
			{
				cry_sprintf(tmp, " sym%i", *sit);
				out += tmp;
			}
			file.WriteLineToFile("Callstacks.txt", out.c_str());
		}

		// Threads.txt
		for (SStrobosopeResult::TThreadInfo::const_iterator it = res.ThreadInfo.begin(), end = res.ThreadInfo.end(); it != end; ++it)
			file.WriteLineToFile("Threads.txt", "%u %.6f %i %s", it->Id, it->Samples, it->Samples, it->Name.c_str());

		// Frames.txt
		file.WriteLineToFile("Frames.txt", "%i %i", res.StartFrame, res.EndFrame);
		for (SStrobosopeResult::TFrameTime::const_iterator it = res.FrameTime.begin(), end = res.FrameTime.end(); it != end; ++it)
			file.WriteLineToFile("Frames.txt", "%i %.6f", it->first, it->second);

		// Version info
		file.WriteLineToFile("Version 0.84.CRY required", "0.84.CRY");
	}
	else
	{
		gEnv->pLog->LogWarning("[Stroboscope] Dump Stroboscope profile results failed (invalid results)!");
	}
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
void CStroboscope::StartProfilerCmd(IConsoleCmdArgs* pArgs)
{
	CStroboscope* pInst = GetInst();
	float duration = -1;
	float startDelay = 0;
	int throttle = 100;
	SThreadInfo::TThreadIds threadIds;
	if (pArgs->GetArgCount() > 1)
	{
		const char* arg = pArgs->GetArg(1);
		duration = (float)atof(arg);
	}
	if (pArgs->GetArgCount() > 2)
	{
		const char* arg = pArgs->GetArg(2);
		startDelay = (float)atof(arg);
	}

	if (pArgs->GetArgCount() > 3)
	{
		const char* arg = pArgs->GetArg(3);
		throttle = atoi(arg);
	}

	for (int i = 4, end = pArgs->GetArgCount(); i < end; ++i)
	{
		char* stopAt;
		const char* arg = pArgs->GetArg(i);
		threadIds.push_back(strtoul(arg, &stopAt, 10));
	}

	pInst->StartProfiling(startDelay, duration, throttle, threadIds);
}

////////////////////////////////////////////////////////////////////////////
void CStroboscope::StopProfilerCmd(IConsoleCmdArgs* pArgs)
{
	CStroboscope* pInst = GetInst();
	pInst->StopProfiling();
}

////////////////////////////////////////////////////////////////////////////
void CStroboscope::DumpResultsCmd(IConsoleCmdArgs* pArgs)
{
	CStroboscope* pInst = GetInst();
	pInst->DumpOutputResult();
}

#endif //#if defined(ENABLE_PROFILING_CODE)
