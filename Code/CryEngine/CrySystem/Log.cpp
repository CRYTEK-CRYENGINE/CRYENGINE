// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File:Log.cpp
//  Description:Log related functions
//
//	History:
//	-Feb 2,2001:Created by Marco Corbetta
//
//////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "Log.h"

//this should not be included here
#include <CrySystem/IConsole.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/IStreamEngine.h>
#include <CryNetwork/INetwork.h>  // EvenBalance - M. Quinn
#include "System.h"
#include <CryString/CryPath.h>          // PathUtil::ReplaceExtension()
#include <CryGame/IGameFramework.h>
#include <CryString/UnicodeFunctions.h>
#include <CryString/StringUtils.h>

#if CRY_PLATFORM_WINDOWS
	#include <time.h>
#endif

#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
	#include <syslog.h>
#endif

#if CRY_PLATFORM_IOS
	#include "SystemUtilsApple.h"
#endif

#define LOG_EXCLUSIVE_ACCESS_SINGLE_WRITER_LOCK_VALUE (uint32)BIT(31) // sets last bit to indicate readers must wait on writer

//////////////////////////////////////////////////////////////////////////
ILINE void LockNoneExclusiveAccess(SExclusiveThreadAccessLock* pExclusiveLock)
{
	const threadID nCurrentThreadId = CryGetCurrentThreadId();

	// Add reader count
	volatile uint32 nCurCount = 0;
	do
	{
		// Spin until writer thread has finished
		while ((nCurCount = pExclusiveLock->counter) >= LOG_EXCLUSIVE_ACCESS_SINGLE_WRITER_LOCK_VALUE)
		{
			// Except we are the writer thread
			if (pExclusiveLock->writerThreadId == nCurrentThreadId)
			{
				break; // Break out of for loop BUT not while loop (which still increments counter)
			}

			CrySleep(0);
		}

	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&pExclusiveLock->counter), nCurCount + 1, nCurCount) != nCurCount);
}

//////////////////////////////////////////////////////////////////////////
ILINE void UnlockNoneExclusiveAccess(SExclusiveThreadAccessLock* pExclusiveLock)
{
	CryInterlockedDecrement(alias_cast<volatile int*>(&pExclusiveLock->counter));
}

//////////////////////////////////////////////////////////////////////////
ILINE void LockExclusiveAccess(SExclusiveThreadAccessLock* pExclusiveLock)
{
	const threadID nCurrentThreadId = static_cast<threadID>(CryGetCurrentThreadId());
	volatile uint32 nCurCount = 0;
	volatile uint32 nCurCountNoLock = 0;

	if (pExclusiveLock->writerThreadId == nCurrentThreadId)
	{
		CryFatalError("Thread has already acquired lock. ThreadId: %" PRI_THREADID, nCurrentThreadId);
	}

	do // Get writer lock
	{
		nCurCount = pExclusiveLock->counter;
		nCurCountNoLock = nCurCount & (~LOG_EXCLUSIVE_ACCESS_SINGLE_WRITER_LOCK_VALUE);
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&pExclusiveLock->counter), (nCurCount | LOG_EXCLUSIVE_ACCESS_SINGLE_WRITER_LOCK_VALUE), nCurCountNoLock) != nCurCountNoLock);

	pExclusiveLock->writerThreadId = nCurrentThreadId;

	while (pExclusiveLock->counter != LOG_EXCLUSIVE_ACCESS_SINGLE_WRITER_LOCK_VALUE)
	{
		CrySleep(0);
	}
}

//////////////////////////////////////////////////////////////////////////
ILINE void UnlockExclusiveAccess(SExclusiveThreadAccessLock* pExclusiveLock)
{
	pExclusiveLock->writerThreadId = THREADID_NULL;

	volatile uint32 nCurCount = 0;
	do // Release writer lock
	{
		nCurCount = pExclusiveLock->counter;
	}
	while (CryInterlockedCompareExchange(alias_cast<volatile LONG*>(&pExclusiveLock->counter), (nCurCount & (~LOG_EXCLUSIVE_ACCESS_SINGLE_WRITER_LOCK_VALUE)), nCurCount) != nCurCount);
}

#undef LOG_EXCLUSIVE_ACCESS_SINGLE_WRITER_LOCK_VALUE

//////////////////////////////////////////////////////////////////////
namespace LogCVars
{
float s_log_tick = 0;
};

#ifndef _RELEASE
static CLog::LogStringType indentString("    ");
#endif

//////////////////////////////////////////////////////////////////////
CLog::CLog(ISystem* pSystem)
	: m_pSystem(pSystem)
	, m_fLastLoadingUpdateTime(-1.0f)
	, m_pLogFile(nullptr)
	, m_pErrFile(nullptr)
	, m_nErrCount(0)
#if defined(SUPPORT_LOG_IDENTER)
	, m_indentation(0)
	, m_topIndenter(nullptr)
#endif
	, m_pLogIncludeTime(nullptr)
	, m_pConsole(nullptr)
	, m_iLastHistoryItem(0)
#if KEEP_LOG_FILE_OPEN
	, m_bFirstLine(true)
#endif
	, m_pLogVerbosity(nullptr)
	, m_pLogWriteToFile(nullptr)
	, m_pLogWriteToFileVerbosity(nullptr)
	, m_pLogVerbosityOverridesWriteToFile(nullptr)
	, m_pLogSpamDelay(nullptr)
	, m_pLogModule(nullptr)
	, m_eLogMode(eLogMode_Normal)
{
	memset(m_szFilename, 0, MAX_FILENAME_SIZE);
	memset(m_sBackupFilename, 0, MAX_FILENAME_SIZE);

	m_nMainThreadId = CryGetCurrentThreadId();
}

void CLog::RegisterConsoleVariables()
{
	IConsole* console = m_pSystem->GetIConsole();

#ifdef  _RELEASE
	#if defined(RELEASE_LOGGING)
		#define DEFAULT_VERBOSITY 0
	#elif defined(DEDICATED_SERVER)
		#define DEFAULT_VERBOSITY 0
	#else
		#define DEFAULT_VERBOSITY -1
	#endif
#else
	#define DEFAULT_VERBOSITY 3
#endif

	if (console)
	{

		m_pLogVerbosity = REGISTER_INT("log_Verbosity", DEFAULT_VERBOSITY, VF_DUMPTODISK,
		                               "defines the verbosity level for log messages written to console\n"
		                               "-1=suppress all logs (including eAlways)\n"
		                               "0=suppress all logs(except eAlways)\n"
		                               "1=additional errors\n"
		                               "2=additional warnings\n"
		                               "3=additional messages\n"
		                               "4=additional comments");

		//writing to game.log during game play causes stalls on consoles
		m_pLogWriteToFile = REGISTER_INT("log_WriteToFile", 1, VF_DUMPTODISK, "toggle whether to write log to file (game.log)");

		m_pLogWriteToFileVerbosity = REGISTER_INT("log_WriteToFileVerbosity", DEFAULT_VERBOSITY, VF_DUMPTODISK,
		                                          "defines the verbosity level for log messages written to files\n"
		                                          "-1=suppress all logs (including eAlways)\n"
		                                          "0=suppress all logs(except eAlways)\n"
		                                          "1=additional errors\n"
		                                          "2=additional warnings\n"
		                                          "3=additional messages\n"
		                                          "4=additional comments");
		m_pLogVerbosityOverridesWriteToFile = REGISTER_INT("log_VerbosityOverridesWriteToFile", 1, VF_DUMPTODISK, "when enabled, setting log_verbosity to 0 will stop all logging including writing to file");

		// put time into begin of the string if requested by cvar
		m_pLogIncludeTime = REGISTER_INT("log_IncludeTime", 1, 0,
		                                 "Toggles time stamping of log entries.\n"
		                                 "Usage: log_IncludeTime [0/1/2/3/4/5]\n"
		                                 "  0=off (default)\n"
		                                 "  1=current time\n"
		                                 "  2=relative time\n"
		                                 "  3=current+relative time\n"
		                                 "  4=absolute time in seconds since this mode was started\n"
		                                 "  5=current time+server time");

		m_pLogSpamDelay = REGISTER_FLOAT("log_SpamDelay", 0.0f, 0, "Sets the minimum time interval between messages classified as spam");

		m_pLogModule = REGISTER_STRING("log_Module", "", VF_NULL, "Only show warnings from specified module");

		REGISTER_CVAR2("log_tick", &LogCVars::s_log_tick, LogCVars::s_log_tick, 0, "When not 0, writes tick log entry into the log file, every N seconds");

#if KEEP_LOG_FILE_OPEN
		REGISTER_COMMAND("log_flush", &LogFlushFile, 0, "Flush the log file");
#endif
	}
	/*
	   //testbed
	   {
	    int iSave0 = m_pLogVerbosity->GetIVal();
	    int iSave1 = m_pLogFileVerbosity->GetIVal();

	    for(int i=0;i<=4;++i)
	    {
	      m_pLogVerbosity->Set(i);
	      m_pLogFileVerbosity->Set(i);

	      LogWithType(eAlways,"CLog selftest: Verbosity=%d FileVerbosity=%d",m_pLogVerbosity->GetIVal(),m_pLogFileVerbosity->GetIVal());
	      LogWithType(eAlways,"--------------");

	      LogWithType(eError,"eError");
	      LogWithType(eWarning,"eWarning");
	      LogWithType(eMessage,"eMessage");
	      LogWithType(eInput,"eInput");
	      LogWithType(eInputResponse,"eInputResponse");

	      LogWarning("LogWarning()");
	      LogError("LogError()");
	      LogWithType(eAlways,"--------------");
	    }

	    m_pLogVerbosity->Set(iSave0);
	    m_pLogFileVerbosity->Set(iSave1);
	   }
	 */
#undef DEFAULT_VERBOSITY
}

//////////////////////////////////////////////////////////////////////
CLog::~CLog()
{
#if defined(SUPPORT_LOG_IDENTER)
	while (m_topIndenter)
	{
		m_topIndenter->Enable(false);
	}

	assert(m_indentation == 0);
#endif

	CreateBackupFile();

	UnregisterConsoleVariables();

	CloseLogFile(true);
}

void CLog::UnregisterConsoleVariables()
{
	m_pLogVerbosity = 0;
	m_pLogWriteToFile = 0;
	m_pLogWriteToFileVerbosity = 0;
	m_pLogVerbosityOverridesWriteToFile = 0;
	m_pLogIncludeTime = 0;
	m_pLogSpamDelay = 0;
}

//////////////////////////////////////////////////////////////////////////
void CLog::CloseLogFile(bool forceClose)
{
	if (m_pLogFile)
	{
		LockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
		fclose(m_pLogFile);
		m_pLogFile = NULL;
		UnlockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
	}
}

//////////////////////////////////////////////////////////////////////////
FILE* CLog::OpenLogFile(const char* filename, const char* mode)
{
	SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

#if CRY_PLATFORM_IOS
	char buffer[1024];
	cry_strcpy(buffer, "");
	if (AppleGetUserLibraryDirectory(buffer, sizeof(buffer)))
	{
		cry_strcat(buffer, "/");
		cry_strcat(buffer, filename);
		LockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
		m_pLogFile = fxopen(buffer, mode);
		UnlockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
	}
	else
	{
		m_pLogFile = NULL;
	}
#else
	LockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
	m_pLogFile = fxopen(filename, mode);
	UnlockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
#endif

	if (m_pLogFile)
	{
#if KEEP_LOG_FILE_OPEN
		m_bFirstLine = true;
		setvbuf(m_pLogFile, m_logBuffer, _IOFBF, sizeof(m_logBuffer));
#endif
	}
	else
	{
#if CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
		syslog(LOG_NOTICE, "Failed to open log file [%s], mode [%s]", filename, mode);
#endif
	}

	return m_pLogFile;
}

//////////////////////////////////////////////////////////////////////////
void CLog::SetVerbosity(int verbosity)
{
	if (m_pLogVerbosity)
		m_pLogVerbosity->Set(verbosity);
}

//////////////////////////////////////////////////////////////////////////
#if !defined(EXCLUDE_NORMAL_LOG)
void CLog::LogWarning(const char* szFormat, ...)
{
	va_list ArgList;
	va_start(ArgList, szFormat);
	LogV(eWarning, szFormat, ArgList);
	va_end(ArgList);
}

//////////////////////////////////////////////////////////////////////////
void CLog::LogError(const char* szFormat, ...)
{
	va_list ArgList;
	va_start(ArgList, szFormat);
	LogV(eError, szFormat, ArgList);
	va_end(ArgList);
}

//////////////////////////////////////////////////////////////////////////
void CLog::Log(const char* szFormat, ...)
{
	va_list arg;
	va_start(arg, szFormat);
	LogV(eMessage, szFormat, arg);
	va_end(arg);
}

//////////////////////////////////////////////////////////////////////////
void CLog::LogAlways(const char* szFormat, ...)
{
	va_list arg;
	va_start(arg, szFormat);
	LogV(eAlways, szFormat, arg);
	va_end(arg);
}
#endif // !defined(EXCLUDE_NORMAL_LOG)

int MatchStrings(const char* str0, const char* str1)
{
	const char* str[] = { str0, str1 };
	int i, bSkipWord, bAlpha[2], bWS[2], bStop = 0, nDiffs = 0, nWordDiffs, len = 0;
	do
	{
		for (i = 0; i < 2; i++) // skip the spaces, stop at 0
			while (*str[i] == ' ')
				if (!*str[i]++)
					goto break2;
		bWS[0] = bWS[1] = nWordDiffs = bSkipWord = 0;
		do
		{
			for (i = bAlpha[0] = bAlpha[1] = 0; i < 2; i++)
				if (!bWS[i])
					do
					{
						int chr = *str[i]++;
						bSkipWord |= iszero(chr - '\\') | iszero(chr - '/') | iszero(chr - '_'); // ignore different words with \,_,/
						bAlpha[i] = inrange(chr, 'A' - 1, 'Z' + 1) | inrange(chr, 'a' - 1, 'z' + 1);
						bWS[i] = iszero(chr - ' ');
						bStop |= iszero(chr);
					}
					while (!(bAlpha[i] | bWS[i] | bStop));
			// wait for a letter or a space in each input string
			if (!bStop)
			{
				len += bAlpha[0] & bAlpha[1];
				nWordDiffs += 1 - iszero((int)(*str[0] - *str[1])) & -(bAlpha[0] & bAlpha[1]); // count diffs in this word
			}
		}
		while ((1 - bWS[0] | 1 - bWS[1]) & 1 - bStop); // wait for space (word end) in both strings
		nDiffs += nWordDiffs & ~-bSkipWord;
	}
	while (!bStop);
break2:
	return nDiffs * 10 < len;
}

//will log the text both to file and console
//////////////////////////////////////////////////////////////////////
void CLog::LogV(const ELogType type, const char* szFormat, va_list args)
{
	LogV(type, 0, szFormat, args);
}

void CLog::LogV(const ELogType type, int flags, const char* szFormat, va_list args)
{
	if (!szFormat)
		return;

	if (m_pLogVerbosityOverridesWriteToFile && m_pLogVerbosityOverridesWriteToFile->GetIVal())
	{
		if (m_pLogVerbosity && m_pLogVerbosity->GetIVal() < 0)
		{
			return;
		}
	}

	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);
	//LOADING_TIME_PROFILE_SECTION(GetISystem());

	bool bfile = false, bconsole = false;
	const char* szCommand = szFormat;

	uint8 DefaultVerbosity = 0; // 0 == Always log (except for special -1 verbosity overrides)

	switch (type)
	{
	case eAlways:
	case eWarningAlways:
	case eErrorAlways:
	case eInput:
	case eInputResponse:
		DefaultVerbosity = 0;
		break;
	case eError:
	case eAssert:
		DefaultVerbosity = 1;
		break;
	case eWarning:
		DefaultVerbosity = 2;
		break;
	case eMessage:
		DefaultVerbosity = 3;
		break;
	case eComment:
		DefaultVerbosity = 4;
		break;
	default:
		CryFatalError("Unsupported ELogType type");
		break;
	}

	szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole, DefaultVerbosity);
	if (!bfile && !bconsole)
	{
		return;
	}

	bool bError = false;

	const char* szPrefix = nullptr;
	switch (type)
	{
	case eWarning:
	case eWarningAlways:
		bError = true;
		szPrefix = "$6[Warning] ";
		break;

	case eError:
	case eErrorAlways:
		bError = true;
		szPrefix = "$4[Error] ";
		break;
	case eAssert:
		bError = true;
		szPrefix = "$4[Assert] ";
	case eMessage:
	case eAlways:
	case eInput:
	case eInputResponse:
	case eComment:
		// Do not modify string
		break;
	default:
		CryFatalError("Unsupported ELogType type");
		break;
	}

	LogStringType formatted;
	formatted.FormatV(szCommand, args);

	if (szPrefix)
	{
		formatted.insert(0, szPrefix);
	}

	if (type == eWarningAlways || type == eWarning || type == eError || type == eErrorAlways)
	{
		const char* sAssetScope = GetAssetScopeString();
		if (*sAssetScope)
		{
			formatted += "\t<Scope> ";
			formatted += sAssetScope;
		}
	}

	float dt;
	const char* szSpamCheck = *szFormat == '%' ? formatted.c_str() : szFormat;
	if (m_pLogSpamDelay && (dt = m_pLogSpamDelay->GetFVal()) > 0.0f && type != eAlways && type != eInputResponse)
	{
		const int sz = CRY_ARRAY_COUNT(m_history);
		int i, j;
		float time = m_pSystem->GetITimer()->GetCurrTime();
		for (i = m_iLastHistoryItem, j = 0; m_history[i].time > time - dt && j < sz; j++, i = i - 1 & sz - 1)
		{
			if (m_history[i].type != type)
				continue;
			if (m_history[i].ptr == szSpamCheck && *(int*)m_history[i].str.c_str() == *(int*)szFormat || MatchStrings(m_history[i].str, szSpamCheck))
				return;
		}
		i = m_iLastHistoryItem = m_iLastHistoryItem + 1 & sz - 1;
		m_history[i].str = m_history[i].ptr = szSpamCheck;
		m_history[i].type = type;
		m_history[i].time = time;
	}

	if (bfile)
	{
		const char* szAfterColor = szPrefix ? formatted.c_str() + 2 : formatted.c_str();
		LogStringToFile(szAfterColor, false, bError);
	}
	if (bconsole)
	{
#ifdef __WITH_PB__
		// Send the console output to PB for audit purposes
		if (gEnv->pNetwork)
			gEnv->pNetwork->PbCaptureConsoleLog(szBuffer, strlen(szBuffer));
#endif
		LogStringToConsole(formatted.c_str());
	}

	switch (type)
	{
	case eAlways:
	case eInput:
	case eInputResponse:
	case eComment:
	case eMessage:
		GetISystem()->GetIRemoteConsole()->AddLogMessage(formatted.c_str());
		break;
	case eWarning:
	case eWarningAlways:
		GetISystem()->GetIRemoteConsole()->AddLogWarning(formatted.c_str());
		break;
	case eError:
	case eErrorAlways:
	case eAssert:
		GetISystem()->GetIRemoteConsole()->AddLogError(formatted.c_str());
		break;
	default:
		CryFatalError("Unsupported ELogType type");
		break;
	}

	//////////////////////////////////////////////////////////////////////////
	if (type == eWarningAlways || type == eWarning || type == eError || type == eErrorAlways)
	{
		IValidator* pValidator = m_pSystem->GetIValidator();
		if (pValidator && (flags & VALIDATOR_FLAG_SKIP_VALIDATOR) == 0)
		{
			CryAutoCriticalSection scope_lock(m_logCriticalSection);

			SValidatorRecord record;
			record.text = formatted.c_str();
			record.module = VALIDATOR_MODULE_SYSTEM;
			record.severity = VALIDATOR_WARNING;
			record.assetScope = GetAssetScopeString();
			record.flags = flags;
			if (type == eError || type == eErrorAlways)
			{
				record.severity = VALIDATOR_ERROR;
			}
			pValidator->Report(record);
		}
	}
}

//will log the text both to the end of file and console
//////////////////////////////////////////////////////////////////////
#if !defined(EXCLUDE_NORMAL_LOG)
void CLog::LogPlus(const char* szFormat, ...)
{
	if (m_pLogVerbosity && m_pLogVerbosity->GetIVal() < 0)
	{
		return;
	}

	if (m_pLogSpamDelay && m_pLogSpamDelay->GetFVal())
	{
		// Vlad: SpamDelay does not work correctly with LogPlus
		return;
	}

	LOADING_TIME_PROFILE_SECTION(GetISystem());

	if (!szFormat)
		return;

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bfile && !bconsole)
		return;

	LogStringType temp;
	va_list arglist;
	va_start(arglist, szFormat);
	temp.FormatV(szCommand, arglist);
	va_end(arglist);

	if (bfile)
	{
		LogStringToFile(temp.c_str(), true);
	}
	if (bconsole)
	{
		LogStringToConsole(temp.c_str(), true);
	}
}

//log to console only
//////////////////////////////////////////////////////////////////////
void CLog::LogStringToConsole(const char* szString, bool bAdd)
{
	// Do not write to console on application crash
	if (m_eLogMode == eLogMode_AppCrash)
		return;

	#if defined(_RELEASE) && defined(EXCLUDE_NORMAL_LOG) // no console logging in release
	return;
	#endif

	//////////////////////////////////////////////////////////////////////////
	if (CryGetCurrentThreadId() != m_nMainThreadId)
	{
		// When logging from other thread then main, push all log strings to queue.
		SLogMsg msg;
		msg.msg = szString;
		msg.bAdd = bAdd;
		msg.bError = false;
		msg.bConsole = true;
		// don't try to store the log message for later in case of out of memory, since then its very likely that this allocation
		// also fails and results in a stack overflow. This way we should at least get a out of memory on-screen message instead of
		// a not obvious crash
		if (gEnv->bIsOutOfMemory == false)
		{
			m_threadSafeMsgQueue.push(msg);
		}
		return;
	}
	//////////////////////////////////////////////////////////////////////////

	if (!m_pSystem || !szString)
	{
		return;
	}

	IConsole* console = m_pSystem->GetIConsole();
	if (!console)
		return;

	LogStringType tempString;
	tempString = szString;

	// add \n at end.
	if (tempString.length() > 0 && tempString[tempString.length() - 1] != '\n')
	{
		tempString += '\n';
	}

	if (bAdd)
	{
		console->PrintLinePlus(tempString.c_str());
	}
	else
	{
		console->PrintLine(tempString.c_str());
	}

	// Call callback function.
	if (!m_callbacks.empty())
	{
		for (Callbacks::iterator it = m_callbacks.begin(); it != m_callbacks.end(); ++it)
		{
			(*it)->OnWriteToConsole(tempString.c_str(), !bAdd);
		}
	}
}

//log to console only
//////////////////////////////////////////////////////////////////////
void CLog::LogToConsole(const char* szFormat, ...)
{
	if (m_pLogVerbosity && m_pLogVerbosity->GetIVal() < 0)
	{
		return;
	}

	if (!szFormat)
	{
		return;
	}

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bconsole)
	{
		return;
	}

	LogStringType temp;
	va_list arglist;
	va_start(arglist, szFormat);
	temp.FormatV(szCommand, arglist);
	va_end(arglist);

	LogStringToConsole(temp.c_str());
}

//////////////////////////////////////////////////////////////////////
void CLog::LogToConsolePlus(const char* szFormat, ...)
{
	if (m_pLogVerbosity && m_pLogVerbosity->GetIVal() < 0)
	{
		return;
	}

	if (!szFormat)
	{
		return;
	}

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bconsole)
	{
		return;
	}

	if (!m_pSystem)
	{
		return;
	}

	LogStringType temp;
	va_list arglist;
	va_start(arglist, szFormat);
	temp.FormatV(szCommand, arglist);
	va_end(arglist);

	LogStringToConsole(temp.c_str(), true);
}
#endif // !defined(EXCLUDE_NORMAL_LOG)

//////////////////////////////////////////////////////////////////////
static void RemoveColorCodeInPlace(CLog::LogStringType& rStr)
{
	char* s = (char*)rStr.c_str();
	char* d = s;

	while (*s != 0)
	{
		if (*s == '$' && *(s + 1) >= '0' && *(s + 1) <= '9')
		{
			s += 2;
			continue;
		}

		*d++ = *s++;
	}
	*d = 0;

	rStr.resize(d - rStr.c_str());
}

#if defined(SUPPORT_LOG_IDENTER)
//////////////////////////////////////////////////////////////////////
void CLog::BuildIndentString()
{
	m_indentWithString = "";

	for (uint8 i = 0; i < m_indentation; ++i)
	{
		m_indentWithString = indentString + m_indentWithString;
	}
}

//////////////////////////////////////////////////////////////////////
void CLog::Indent(CLogIndenter* indenter)
{
	indenter->SetNextIndenter(m_topIndenter);
	m_topIndenter = indenter;
	++m_indentation;
	BuildIndentString();
}

//////////////////////////////////////////////////////////////////////
void CLog::Unindent(CLogIndenter* indenter)
{
	assert(indenter == m_topIndenter);
	assert(m_indentation);
	m_topIndenter = m_topIndenter->GetNextIndenter();
	--m_indentation;
	BuildIndentString();
}

//////////////////////////////////////////////////////////////////////////
void CLog::PushAssetScopeName(const char* sAssetType, const char* sName)
{
	assert(sAssetType);
	assert(sName);

	SAssetScopeInfo as;
	as.sType = sAssetType;
	as.sName = sName;
	CryAutoCriticalSection scope_lock(m_assetScopeQueueLock);
	m_assetScopeQueue.push_back(as);
}

void CLog::PopAssetScopeName()
{
	CryAutoCriticalSection scope_lock(m_assetScopeQueueLock);
	assert(!m_assetScopeQueue.empty());
	if (!m_assetScopeQueue.empty())
	{
		m_assetScopeQueue.pop_back();
	}
}

//////////////////////////////////////////////////////////////////////////
const char* CLog::GetAssetScopeString()
{
	CryAutoCriticalSection scope_lock(m_assetScopeQueueLock);

	m_assetScopeString.clear();
	for (size_t i = 0; i < m_assetScopeQueue.size(); i++)
	{
		m_assetScopeString += "[";
		m_assetScopeString += m_assetScopeQueue[i].sType;
		m_assetScopeString += "]";
		m_assetScopeString += m_assetScopeQueue[i].sName;
		if (i < m_assetScopeQueue.size() - 1)
		{
			m_assetScopeString += " > ";
		}
	}
	return m_assetScopeString.c_str();
};
#endif

//////////////////////////////////////////////////////////////////////
#if !defined(EXCLUDE_NORMAL_LOG)
void CLog::LogStringToFile(const char* szString, bool bAdd, bool bError)
{
	#if defined(_RELEASE) && defined(EXCLUDE_NORMAL_LOG) // no file logging in release
	return;
	#endif

	if (!szString)
	{
		return;
	}

	//////////////////////////////////////////////////////////////////////////
	if (CryGetCurrentThreadId() != m_nMainThreadId && m_eLogMode != eLogMode_AppCrash)
	{
		// When logging from other thread then main, push all log strings to queue.
		SLogMsg msg;
		msg.msg = szString;
		msg.bAdd = bAdd;
		msg.bError = bError;
		msg.bConsole = false;
		// don't try to store the log message for later in case of out of memory, since then its very likely that this allocation
		// also fails and results in a stack overflow. This way we should at least get a out of memory on-screen message instead of
		// a not obvious crash
		if (gEnv->bIsOutOfMemory == false)
		{
			m_threadSafeMsgQueue.push(msg);
		}
		return;
	}
	//////////////////////////////////////////////////////////////////////////

	if (!m_pSystem)
	{
		return;
	}

	LogStringType tempString;
	tempString = szString;

	// Skip any non character.
	if (tempString.length() > 0 && tempString.at(0) < 32)
	{
		tempString.erase(0, 1);
	}

	RemoveColorCodeInPlace(tempString);

	#if defined(SUPPORT_LOG_IDENTER)
	if (m_topIndenter)
	{
		m_topIndenter->DisplaySectionText();
	}

	tempString = m_indentWithString + tempString;
	#endif

	if (m_pLogIncludeTime && gEnv->pTimer)
	{
		uint32 dwCVarState = m_pLogIncludeTime->GetIVal();
		char sTime[21];
		if (dwCVarState == 5) // Log_IncludeTime
		{
			if (gEnv->pGameFramework)
			{
				CTimeValue serverTime = gEnv->pGameFramework->GetServerTime();
				cry_sprintf(sTime, "<%.2f> ", serverTime.GetSeconds());
				tempString.insert(0, sTime);
			}
			dwCVarState = 1; // Afterwards insert time as-if Log_IncludeTime == 1
		}
		if (dwCVarState < 4)
		{
			if (dwCVarState & 1) // Log_IncludeTime
			{
				time_t ltime;
				time(&ltime);
				struct tm* today = localtime(&ltime);
				strftime(sTime, CRY_ARRAY_COUNT(sTime), "<%H:%M:%S> ", today);
				sTime[CRY_ARRAY_COUNT(sTime) - 1] = 0;
				tempString.insert(0, sTime);
			}
			if (dwCVarState & 2) // Log_IncludeTime
			{
				static CTimeValue lasttime;
				const CTimeValue currenttime = gEnv->pTimer->GetAsyncTime();
				if (lasttime != CTimeValue())
				{
					const uint32 dwMs = (uint32)((currenttime - lasttime).GetMilliSeconds());
					cry_sprintf(sTime, "<%3u.%.3u>: ", dwMs / 1000, dwMs % 1000);
					tempString.insert(0, sTime);
				}
				lasttime = currenttime;
			}
		}
		else if (dwCVarState == 4) // Log_IncludeTime
		{
			static bool bFirst = true;
			static CTimeValue lasttime;
			const CTimeValue currenttime = gEnv->pTimer->GetAsyncTime();
			if (lasttime != CTimeValue())
			{
				const uint32 dwMs = (uint32)((currenttime - lasttime).GetMilliSeconds());
				cry_sprintf(sTime, "<%3u.%.3u>: ", dwMs / 1000, dwMs % 1000);
				tempString.insert(0, sTime);
			}
			if (bFirst)
			{
				lasttime = currenttime;
				bFirst = false;
			}
		}
	}

	#if !KEEP_LOG_FILE_OPEN
	// add \n at end.
	if (tempString.empty() || tempString[tempString.length() - 1] != '\n')
	{
		tempString += '\n';
	}
	#endif

	//////////////////////////////////////////////////////////////////////////
	// Call callback function (on invoke if we are not in application crash)
	if (!m_callbacks.empty() && m_eLogMode != eLogMode_AppCrash)
	{
		for (Callbacks::iterator it = m_callbacks.begin(); it != m_callbacks.end(); ++it)
		{
			(*it)->OnWriteToFile(tempString.c_str(), !bAdd);
		}
	}
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// Write to file.
	//////////////////////////////////////////////////////////////////////////
	int logToFile = m_pLogWriteToFile ? m_pLogWriteToFile->GetIVal() : 1;

	if (logToFile)
	{
		SCOPED_ALLOW_FILE_ACCESS_FROM_THIS_THREAD();

	#if KEEP_LOG_FILE_OPEN
		if (!m_pLogFile)
		{
			OpenLogFile(m_szFilename, "at");
		}

		if (m_pLogFile)
		{
			if (m_bFirstLine)
			{
				m_bFirstLine = false;
			}
			else
			{
				if (!bAdd)
				{
					tempString = "\n" + tempString;
				}
			}

			fputs(tempString.c_str(), m_pLogFile);

			if (m_pLogFile)
			{
				fflush(m_pLogFile); // Flush or the changes will only show up on shutdown.
			}
		}
	#else
		if (bAdd)
		{
			FILE* fp = OpenLogFile(m_szFilename, "r+t");
			if (fp)
			{
				int nRes;
				nRes = fseek(fp, 0, SEEK_END);
				assert(nRes == 0);
				nRes = fseek(fp, -2, SEEK_CUR);
				assert(nRes == 0);
				(void)nRes;
				fputs(tempString.c_str(), fp);
				CloseLogFile();
			}
		}
		else
		{
			// comment on bug by TN: Log file misses the last lines of output
			// Temporally forcing the Log to flush before closing the file, so all lines will show up
			if (FILE* fp = OpenLogFile(m_szFilename, "at")) // change to option "atc"
			{
				fputs(tempString.c_str(), fp);
				// fflush(fp);  // enable to flush the file
				CloseLogFile();
			}
		}
	#endif
	}

	#if !defined(_RELEASE)
		#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO
	// Note: OutputDebugStringW will not actually output Unicode unless the attached debugger has explicitly opted in to this behavior.
	// This is only possible on Windows 10; on older operating systems the W variant internally converts the input to the local codepage (ANSI) and calls the A variant.
	// Both VS2015 and VS2017 do opt-in to this behavior on Windows 10, so we use the W variant despite the slight overhead on older Windows versions.
	wstring tempWString = CryStringUtils::UTF8ToWStr(tempString);
	OutputDebugStringW(tempWString.c_str());
		#else
	OutputDebugString(tempString.c_str());
		#endif
	#endif
}

//same as above but to a file
//////////////////////////////////////////////////////////////////////
void CLog::LogToFilePlus(const char* szFormat, ...)
{
	if (m_pLogVerbosity && m_pLogVerbosity->GetIVal() < 0)
	{
		return;
	}

	if (!m_szFilename[0] || !szFormat)
	{
		return;
	}

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bfile)
		return;

	LogStringType temp;
	va_list arglist;
	va_start(arglist, szFormat);
	temp.FormatV(szCommand, arglist);
	va_end(arglist);

	LogStringToFile(temp.c_str(), true);
}

//log to the file specified in setfilename
//////////////////////////////////////////////////////////////////////
void CLog::LogToFile(const char* szFormat, ...)
{
	if (m_pLogVerbosity && m_pLogVerbosity->GetIVal() < 0)
	{
		return;
	}

	if (!m_szFilename[0] || !szFormat)
	{
		return;
	}

	bool bfile = false, bconsole = false;
	const char* szCommand = CheckAgainstVerbosity(szFormat, bfile, bconsole);
	if (!bfile)
		return;

	LogStringType temp;
	va_list arglist;
	va_start(arglist, szFormat);
	temp.FormatV(szCommand, arglist);
	va_end(arglist);

	LogStringToFile(temp.c_str(), false);
}
#endif // !defined(EXCLUDE_NORMAL_LOG)

//////////////////////////////////////////////////////////////////////
void CLog::CreateBackupFile() const
{
	LOADING_TIME_PROFILE_SECTION;
#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE || CRY_PLATFORM_DURANGO

	if (!gEnv->pCryPak)
	{
		return;
	}

	const string srcFilePath = m_szFilename;
	const string srcFileName = PathUtil::GetFileName(srcFilePath);
	const string srcFileExt = PathUtil::GetExt(srcFilePath);
	const string logBackupFolder = "LogBackups";
	gEnv->pCryPak->MakeDir(logBackupFolder);

	LockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
	FILE* src = fxopen(srcFilePath, "rb");
	UnlockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);

	string sBackupNameAttachment;

	// parse backup name attachment
	// e.g. BackupNameAttachment="attachment name"
	if (src)
	{
		bool bKeyFound = false;
		string sName;

		while (!feof(src))
		{
			uint8 c = fgetc(src);

			if (c == '\"')
			{
				if (!bKeyFound)
				{
					bKeyFound = true;

					if (sName.find("BackupNameAttachment=") == string::npos)
					{
	#if CRY_PLATFORM_WINDOWS
						OutputDebugString("Log::CreateBackupFile ERROR '");
						OutputDebugString(sName.c_str());
						OutputDebugString("' not recognized \n");
	#endif
						assert(0);    // broken log file? - first line should include this name - written by LogVersion()
						return;
					}
					sName.clear();
				}
				else
				{
					sBackupNameAttachment = sName;
					break;
				}
				continue;
			}
			if (c >= ' ')
				sName += c;
			else
				break;
		}
		LockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
		fclose(src);
		UnlockNoneExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
	}

	const string dstFilePath = PathUtil::Make(logBackupFolder, srcFileName + sBackupNameAttachment + "." + srcFileExt);
	
#if CRY_PLATFORM_DURANGO
	// Xbox has some limitation in file names. No spaces in file name are allowed. The full path is limited by MAX_PATH, etc.
	// I change spaces with underscores here for valid name and cut it if it exceed a limit.
	auto processDurangoPath = [](string path)
	{
		path = PathUtil::ToDosPath(path);
		path.replace(' ', '_');
		if (path.size() > MAX_PATH)
			path.resize(MAX_PATH);
		return CryStringUtils::UTF8ToWStrSafe(path);
	};
	const wstring durangoSrcFilePath = processDurangoPath(srcFilePath);
	const wstring durangosDstFilePath = processDurangoPath(dstFilePath);
	HRESULT result = CopyFile2(durangoSrcFilePath, durangosDstFilePath, nullptr);
	CRY_ASSERT_MESSAGE(result == S_OK, "Error copying log backup file");
#else
	CopyFile(srcFilePath, dstFilePath, false);
#endif

#endif
}

//set the file used to log to disk
//////////////////////////////////////////////////////////////////////
bool CLog::SetFileName(const char* filename)
{
	if (!filename)
	{
		return false;
	}

	string temp = PathUtil::Make(m_pSystem->GetRootFolder(), PathUtil::GetFile(filename));
	if (temp.empty() || temp.size() >= sizeof(m_szFilename))
		return false;

	cry_strcpy(m_szFilename, temp.c_str());

	CreateBackupFile();

	FILE* fp = OpenLogFile(m_szFilename, "wt");
	if (fp)
	{
		CloseLogFile(true);
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
const char* CLog::GetFileName() const
{
	return m_szFilename;
}

const char* CLog::GetBackupFileName() const
{
	return m_sBackupFilename;
}

//////////////////////////////////////////////////////////////////////
void CLog::UpdateLoadingScreen(const char* szFormat, ...)
{
#if !defined(EXCLUDE_NORMAL_LOG)
	if (szFormat)
	{
		va_list args;
		va_start(args, szFormat);

		LogV(ILog::eMessage, szFormat, args);

		va_end(args);
	}
#endif

	if (CryGetCurrentThreadId() == m_nMainThreadId)
	{
		((CSystem*)m_pSystem)->UpdateLoadingScreen();

#if !(CRY_PLATFORM_LINUX || CRY_PLATFORM_ANDROID)
		// Take this opportunity to update streaming engine.
		if (IStreamEngine* pStreamEngine = GetISystem()->GetStreamEngine())
		{
			const float curTime = m_pSystem->GetITimer()->GetAsyncCurTime();
			if (curTime - m_fLastLoadingUpdateTime > .1f)  // not frequent than once in 100ms
			{
				m_fLastLoadingUpdateTime = curTime;
				pStreamEngine->Update();
			}
		}
#endif
	}
}

//////////////////////////////////////////////////////////////////////////
int CLog::GetVerbosityLevel() const
{
	if (m_pLogVerbosity)
	{
		return (m_pLogVerbosity->GetIVal());
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Checks the verbosity of the message and returns NULL if the message must NOT be
// logged, or the pointer to the part of the message that should be logged
// NOTE:
//    Normally, this is either the pText pointer itself, or the pText+1, meaning
//    the first verbosity character may be cut off)
//    This is done in order to avoid modification of const char*, which may cause GPF
//    sometimes, or kill the verbosity qualifier in the text that's gonna be passed next time.
const char* CLog::CheckAgainstVerbosity(const char* pText, bool& logtofile, bool& logtoconsole, const uint8 DefaultVerbosity)
{
	// the max verbosity (most detailed level)
#if defined(RELEASE)
	const unsigned char nMaxVerbosity = 0;
#else // #if defined(RELEASE)
	const unsigned char nMaxVerbosity = 8;
#endif // #if defined(RELEASE)

	// the current verbosity of the log
	int nLogVerbosityConsole = m_pLogVerbosity ? m_pLogVerbosity->GetIVal() : nMaxVerbosity;
	int nLogVerbosityFile = m_pLogWriteToFileVerbosity ? m_pLogWriteToFileVerbosity->GetIVal() : nMaxVerbosity;

	logtoconsole = (nLogVerbosityConsole >= DefaultVerbosity);

	//to preserve logging to TTY, logWriteToFile logic has been moved to inside logStringToFile
	//int logToFileCVar = m_pLogWriteToFile ? m_pLogWriteToFile->GetIVal() : 1;

	logtofile = (nLogVerbosityFile >= DefaultVerbosity);

	return pText;
}

//////////////////////////////////////////////////////////////////////////
void CLog::AddCallback(ILogCallback* pCallback)
{
	stl::push_back_unique(m_callbacks, pCallback);
}

//////////////////////////////////////////////////////////////////////////
void CLog::RemoveCallback(ILogCallback* pCallback)
{
	m_callbacks.remove(pCallback);
}

//////////////////////////////////////////////////////////////////////////
void CLog::Update()
{
	CRY_PROFILE_FUNCTION(PROFILE_SYSTEM);

	if (CryGetCurrentThreadId() == m_nMainThreadId)
	{
		auto messages = m_threadSafeMsgQueue.pop_all();
		for (const SLogMsg& msg : messages)
		{
			if (msg.bConsole)
				LogStringToConsole(msg.msg, msg.bAdd);
			else
				LogStringToFile(msg.msg, msg.bAdd, msg.bConsole);
		}

		if (LogCVars::s_log_tick != 0)
		{
			static CTimeValue t0 = GetISystem()->GetITimer()->GetAsyncTime();
			CTimeValue t1 = GetISystem()->GetITimer()->GetAsyncTime();
			if (fabs((t1 - t0).GetSeconds()) > LogCVars::s_log_tick)
			{
				t0 = t1;

				char sTime[128];
				time_t ltime;
				time(&ltime);
				struct tm* today = localtime(&ltime);
				strftime(sTime, sizeof(sTime) - 1, "<%H:%M:%S> ", today);
				LogAlways("<tick> %s", sTime);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const char* CLog::GetModuleFilter()
{
	if (m_pLogModule)
	{
		return m_pLogModule->GetString();
	}
	return "";
}

void CLog::Flush()
{
	Update();
#if KEEP_LOG_FILE_OPEN
	if (m_pLogFile)
	{
		fflush(m_pLogFile);
	}
#endif
}

void CLog::FlushAndClose()
{
	Update();
#if KEEP_LOG_FILE_OPEN
	if (m_pLogFile)
	{
		CloseLogFile(true);
	}
#endif
}

#if KEEP_LOG_FILE_OPEN
void CLog::LogFlushFile(IConsoleCmdArgs* pArgs)
{
	gEnv->pLog->Flush();
}
#endif

void CLog::SetLogMode(ELogMode eLogMode)
{
	m_eLogMode = eLogMode;
}

ELogMode CLog::GetLogMode() const
{
	return m_eLogMode;
}

void CLog::ThreadExclusiveLogAccess(bool state)
{
	if (state)
		LockExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
	else
		UnlockExclusiveAccess(&m_exclusiveLogFileThreadAccessLock);
}
