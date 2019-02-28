// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   Sampler.h
//  Version:     v1.00
//  Created:     14/3/2005 by Timur.
//  Compilers:   Visual Studio.NET 2003
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Sampler.h"

#if CRY_PLATFORM_WINDOWS
	#include <CrySystem/ISystem.h>

	#include <CryCore/Platform/CryWindows.h>
	#include <dbghelp.h>  // needs <windows.h> to be defined
	#include <Mmsystem.h> // needs <windows.h> to be defined

	#define MAX_SYMBOL_LENGTH 512

//////////////////////////////////////////////////////////////////////////
// Makes thread.
//////////////////////////////////////////////////////////////////////////
class CSamplingThread
{
public:
	CSamplingThread(CSampler* pSampler)
	{
		m_hThread = NULL;
		m_pSampler = pSampler;
		m_bStop = false;
		m_samplePeriodMs = pSampler->GetSamplePeriod();

		m_hProcess = GetCurrentProcess();
		m_hSampledThread = GetCurrentThread();
		DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &m_hSampledThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
	}

	// Start thread.
	void Start();
	void Stop();

protected:
	virtual ~CSamplingThread() {};
	static DWORD WINAPI ThreadFunc(void* pThreadParam);
	void                Run(); // Derived classes must override this.

	HANDLE    m_hProcess;
	HANDLE    m_hThread;
	HANDLE    m_hSampledThread;
	DWORD     m_ThreadId;

	CSampler* m_pSampler;
	bool      m_bStop;
	int       m_samplePeriodMs;
};

//////////////////////////////////////////////////////////////////////////
void CSamplingThread::Start()
{
	m_hThread = CreateThread(NULL, 0, ThreadFunc, this, 0, &m_ThreadId);

}

//////////////////////////////////////////////////////////////////////////
void CSamplingThread::Stop()
{
	m_bStop = true;
}

//////////////////////////////////////////////////////////////////////////
DWORD CSamplingThread::ThreadFunc(void* pThreadParam)
{
	CSamplingThread* thread = (CSamplingThread*)pThreadParam;
	thread->Run();
	// Auto destruct thread class.
	delete thread;
	return 0;
}

//////////////////////////////////////////////////////////////////////////
void CSamplingThread::Run()
{
	//SetThreadPriority( m_hThread,THREAD_PRIORITY_HIGHEST );
	SetThreadPriority(m_hThread, THREAD_PRIORITY_TIME_CRITICAL);
	while (!m_bStop)
	{
		SuspendThread(m_hSampledThread);

		CONTEXT context;
		context.ContextFlags = CONTEXT_CONTROL;

		uint64 ip;
		if (GetThreadContext(m_hSampledThread, &context))
		{
	#ifdef CONTEXT_i386
			ip = context.Eip;
	#else
			ip = context.Rip;
	#endif
			if (ip == 0x7FFE0304) // Special handling for Windows XP sysenter System call.
			{
				// Try to walk stack back.
	#ifdef CONTEXT_i386
				STACKFRAME64 stack_frame;
				memset(&stack_frame, 0, sizeof(stack_frame));
				stack_frame.AddrPC.Mode = AddrModeFlat;
				stack_frame.AddrPC.Offset = context.Eip;
				stack_frame.AddrStack.Mode = AddrModeFlat;
				stack_frame.AddrStack.Offset = context.Esp;
				stack_frame.AddrFrame.Mode = AddrModeFlat;
				stack_frame.AddrFrame.Offset = 0;
				stack_frame.AddrFrame.Offset = context.Ebp;
	#else
				STACKFRAME64 stack_frame;
				memset(&stack_frame, 0, sizeof(stack_frame));
				stack_frame.AddrPC.Mode = AddrModeFlat;
				stack_frame.AddrPC.Offset = context.Rip;
				stack_frame.AddrStack.Mode = AddrModeFlat;
				stack_frame.AddrStack.Offset = context.Rsp;
				stack_frame.AddrFrame.Mode = AddrModeFlat;
				stack_frame.AddrFrame.Offset = 0;
				stack_frame.AddrFrame.Offset = context.Rbp;
	#endif
				if (StackWalk64(IMAGE_FILE_MACHINE_I386, m_hProcess, m_hSampledThread, &stack_frame, &context,
				                NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
				{
					ip = stack_frame.AddrPC.Offset;
				}
				if (ip == 0x7FFE0304)
				{
					// Repeat.
					if (StackWalk64(IMAGE_FILE_MACHINE_I386, m_hProcess, m_hSampledThread, &stack_frame, &context,
					                NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL))
					{
						ip = stack_frame.AddrPC.Offset;
					}
				}
			}
		}
		ResumeThread(m_hSampledThread);

		if (!m_pSampler->AddSample(ip))
			break;

		CrySleep(m_samplePeriodMs);
	}
}

//////////////////////////////////////////////////////////////////////////
CSampler::CSampler()
{
	m_pSamplingThread = NULL;
	SetMaxSamples(2000);
	m_bSamplingFinished = false;
	m_bSampling = false;
	m_pSymDB = 0;
	m_samplePeriodMs = 1; //1ms
}

//////////////////////////////////////////////////////////////////////////
CSampler::~CSampler()
{
}

//////////////////////////////////////////////////////////////////////////
void CSampler::SetMaxSamples(int nMaxSamples)
{
	m_rawSamples.reserve(nMaxSamples);
	m_nMaxSamples = nMaxSamples;
}

//////////////////////////////////////////////////////////////////////////
void CSampler::Start()
{
	if (m_bSampling)
		return;

	CryLogAlways("Staring Sampling with interval %dms, max samples: %d ...", m_samplePeriodMs, m_nMaxSamples);

	if (!m_pSymDB)
	{
		m_pSymDB = new CSymbolDatabase;
		if (!m_pSymDB->Init())
		{
			delete m_pSymDB;
			m_pSymDB = 0;
			return;
		}
	}

	m_bSampling = true;
	m_bSamplingFinished = false;
	m_pSamplingThread = new CSamplingThread(this);
	m_rawSamples.clear();
	m_functionSamples.clear();

	m_pSamplingThread->Start();
}

//////////////////////////////////////////////////////////////////////////
void CSampler::Stop()
{
	if (m_bSamplingFinished)
	{

	}
	if (m_bSampling)
	{
		m_pSamplingThread->Stop();
	}
	m_bSampling = false;
	m_pSamplingThread = 0;
}

//////////////////////////////////////////////////////////////////////////
void CSampler::Update()
{
	if (m_bSamplingFinished)
	{
		ProcessSampledData();
		m_bSamplingFinished = false;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSampler::AddSample(uint64 ip)
{
	if ((int)m_rawSamples.size() >= m_nMaxSamples)
	{
		m_bSamplingFinished = true;
		m_bSampling = false;
		m_pSamplingThread = 0;
		return false;
	}
	m_rawSamples.push_back(ip);
	return true;
}

inline bool CompareFunctionSamples(const CSampler::SFunctionSample& s1, const CSampler::SFunctionSample& s2)
{
	return s1.nSamples < s2.nSamples;
}

//////////////////////////////////////////////////////////////////////////
void CSampler::ProcessSampledData()
{
	CryLogAlways("Processing collected samples...");

	uint32 i;
	// Count duplicates.
	std::map<uint64, int> counts;
	std::map<uint64, int>::iterator cit;
	for (i = 0; i < m_rawSamples.size(); i++)
	{
		uint32 ip = (uint32)m_rawSamples[i];
		cit = counts.find(ip);
		if (cit != counts.end())
			cit->second++;
		else
			counts[ip] = 0;
	}

	if (!m_pSymDB)
	{
		m_pSymDB = new CSymbolDatabase;
		if (!m_pSymDB->Init())
		{
			delete m_pSymDB;
			m_pSymDB = 0;
			return;
		}
	}

	/*
	   struct SingleSampleDescription
	   {
	   uint64 ip;
	   string func;
	   uint32 count;
	   };
	 */

	std::map<string, int> funcCounts;

	string funcName;
	//for (cit = counts.begin(); cit != counts.end(); cit++)
	for (i = 0; i < m_rawSamples.size(); i++)
	{
		/*
		   if (m_pSymDB->LookupFunctionName( cit->first,funcName ))
		   {
		   funcCounts[funcName] += cit->second;
		   }
		 */
		m_pSymDB->LookupFunctionName(m_rawSamples[i], funcName);
		{
			funcCounts[funcName] += 1;
		}
	}

	{
		// Combine function samples.
		std::map<string, int>::iterator it;
		for (it = funcCounts.begin(); it != funcCounts.end(); ++it)
		{
			SFunctionSample fs;
			fs.function = it->first;
			fs.nSamples = it->second;
			m_functionSamples.push_back(fs);
		}
	}

	// Sort vector by number of samples.
	std::sort(m_functionSamples.begin(), m_functionSamples.end(), CompareFunctionSamples);

	LogSampledData();
}

//////////////////////////////////////////////////////////////////////////
void CSampler::LogSampledData()
{
	int nTotalSamples = m_rawSamples.size();

	// Log sample info.
	CryLogAlways("=========================================================================");
	CryLogAlways("= Profiler Output");
	CryLogAlways("=========================================================================");

	float fOnePercent = (float)nTotalSamples / 100;

	float fPercentTotal = 0;
	int nSampleSum = 0;
	for (uint32 i = 0; i < m_functionSamples.size(); i++)
	{
		// Calculate percentage.
		float fPercent = m_functionSamples[i].nSamples / fOnePercent;
		const char* func = m_functionSamples[i].function;
		CryLogAlways("%6.2f%% (%4d samples) : %s", fPercent, m_functionSamples[i].nSamples, func);
		fPercentTotal += fPercent;
		nSampleSum += m_functionSamples[i].nSamples;
	}
	CryLogAlways("Samples: %d / %d (%.2f%%)", nSampleSum, nTotalSamples, fPercentTotal);
	CryLogAlways("=========================================================================");
}

//////////////////////////////////////////////////////////////////////////
//
// CSymbolDatabase Implementation.
//
//////////////////////////////////////////////////////////////////////////
CSymbolDatabase::CSymbolDatabase()
{
	m_bInitialized = false;
}

//////////////////////////////////////////////////////////////////////////
CSymbolDatabase::~CSymbolDatabase()
{
}

//typedef BOOL (CALLBACK *PSYM_ENUMMODULES_CALLBACKW64)( __in PCWSTR ModuleName,__in DWORD64 BaseOfDll,__in_opt PVOID UserContext	);

	#if _MSC_VER >= 1500
BOOL CALLBACK EnumCallcbackM(PCSTR ModuleName, DWORD64 BaseOfDll, PVOID UserContext)
{
	return TRUE;
}
	#else
BOOL WINAPI EnumCallcbackM(PSTR ModuleName, DWORD64 BaseOfDll, PVOID UserContext)
{
	return TRUE;
}
	#endif

//////////////////////////////////////////////////////////////////////////
bool CSymbolDatabase::Init()
{
	char fullpath[_MAX_PATH + 1];
	char pathname[_MAX_PATH + 1];
	char fname[_MAX_PATH + 1];
	char directory[_MAX_PATH + 1];
	char drive[10];
	HANDLE process;

	//	SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_LOAD_LINES|SYMOPT_OMAP_FIND_NEAREST|SYMOPT_INCLUDE_32BIT_MODULES);
	SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_OMAP_FIND_NEAREST);
	//SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_OMAP_FIND_NEAREST);

	process = GetCurrentProcess();

	// Get module file name.
	GetModuleFileName(NULL, fullpath, _MAX_PATH);

	// Convert it into search path for symbols.
	cry_strcpy(pathname, fullpath);
	_splitpath(pathname, drive, directory, fname, NULL);
	cry_sprintf(pathname, "%s%s", drive, directory);

	// Append the current directory to build a search path forSymInit
	cry_strcat(pathname, ";.;");

	int result = 0;

	result = SymInitialize(process, pathname, TRUE);
	if (result)
	{
		char pdb[_MAX_PATH + 1];
		char res_pdb[_MAX_PATH + 1];
		cry_sprintf(pdb, "%s.pdb", fname);
		cry_sprintf(pathname, "%s%s", drive, directory);
		if (SearchTreeForFile(pathname, pdb, res_pdb))
		{
			m_bInitialized = true;
		}
	}
	else
	{
		result = SymInitialize(process, pathname, FALSE);
		if (!result)
		{
			CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_WARNING, "SymInitialize failed");
		}
		else
			m_bInitialized = true;
	}

	SymEnumerateModules64(GetCurrentProcess(), EnumCallcbackM, 0);
	return m_bInitialized;
}

//------------------------------------------------------------------------------------------------------------------------
bool CSymbolDatabase::LookupFunctionName(uint64 ip, string& funcName)
{
	HANDLE process = GetCurrentProcess();
	char symbolBuf[sizeof(SYMBOL_INFO) + MAX_SYMBOL_LENGTH];
	memset(symbolBuf, 0, sizeof(symbolBuf));
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuf;

	DWORD displacement = 0;
	DWORD64 displacement64 = 0;
	pSymbol->SizeOfStruct = sizeof(symbolBuf);
	pSymbol->MaxNameLen = MAX_SYMBOL_LENGTH;
	if (SymFromAddr(process, (DWORD64)ip, &displacement64, pSymbol))
	{
		funcName = string(pSymbol->Name);
		return true;
	}
	else
	{
		IMAGEHLP_MODULE64 moduleInfo;
		memset(&moduleInfo, 0, sizeof(moduleInfo));
		moduleInfo.SizeOfStruct = sizeof(moduleInfo);
		if (SymGetModuleInfo64(process, (DWORD64)ip, &moduleInfo))
		{
			funcName = moduleInfo.ModuleName;
		}
		else
		{
			funcName.Format("%X", (uint32)ip);
		}
	}
	return false;
}

//------------------------------------------------------------------------------------------------------------------------
bool CSymbolDatabase::LookupFunctionName(uint64 ip, string& funcName, string& fileName, int& lineNumber)
{
	HANDLE process = GetCurrentProcess();
	char symbolBuf[sizeof(SYMBOL_INFO) + MAX_SYMBOL_LENGTH];
	memset(symbolBuf, 0, sizeof(symbolBuf));
	PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symbolBuf;

	DWORD displacement = 0;
	DWORD64 displacement64 = 0;
	pSymbol->SizeOfStruct = sizeof(symbolBuf);
	pSymbol->MaxNameLen = MAX_SYMBOL_LENGTH;
	if (SymFromAddr(process, (DWORD64)ip, &displacement64, pSymbol))
	{
		funcName = string(pSymbol->Name);

		// Lookup Line in source file.
		IMAGEHLP_LINE64 lineImg;
		memset(&lineImg, 0, sizeof(lineImg));
		lineImg.SizeOfStruct = sizeof(lineImg);

		if (SymGetLineFromAddr64(process, (DWORD_PTR)ip, &displacement, &lineImg))
		{
			fileName = lineImg.FileName;
			lineNumber = lineImg.LineNumber;
		}
		else
		{
			fileName = "";
			lineNumber = 0;
		}
		return true;
	}
	else
	{
		funcName = "<Unknown>";
		fileName = "<Unknown>";
		lineNumber = 0;
	}
	return false;
}

#endif // CRY_PLATFORM_WINDOWS
