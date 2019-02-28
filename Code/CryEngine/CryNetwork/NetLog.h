// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NETLOG_H__
#define __NETLOG_H__

#pragma once

#include <CrySystem/ILog.h>
#include "Config.h"

extern threadID g_primaryThread;

ILINE bool IsPrimaryThread()
{
	return g_primaryThread == CryGetCurrentThreadId();
}

#if !defined(EXCLUDE_NORMAL_LOG)

void FlushNetLog(bool final);
void InitPrimaryThread();
void NetLog(const char* fmt, ...) PRINTF_PARAMS(1, 2); // CryLog
void VNetLog(const char* pFmt, va_list args);          // CryLog
void NetLogHUD(const char* fmt, ...) PRINTF_PARAMS(1, 2);
// only in response to user input!
void NetLogAlways(const char* fmt, ...) PRINTF_PARAMS(1, 2);        // CryLogAlways
void NetLogAlways_Secret(const char* fmt, ...) PRINTF_PARAMS(1, 2); // CryLogAlways
void NetError(const char* fmt, ...) PRINTF_PARAMS(1, 2);            // CryFatalError
void NetWarning(const char* fmt, ...) PRINTF_PARAMS(1, 2);          // CryWarning
void NetQuickLog(bool toConsole, float timeout, const char* fmt, ...) PRINTF_PARAMS(3, 4);
void NetPerformanceWarning(const char* fmt, ...) PRINTF_PARAMS(1, 2);
void NetComment(const char* fmt, ...) PRINTF_PARAMS(1, 2);    // CryComment

	#if ENABLE_CORRUPT_PACKET_DUMP
void NetLogPacketDebug(const char* fmt, ...) PRINTF_PARAMS(1, 2);
	#else
		#define NetLogPacketDebug(...)
	#endif

string GetBacklogAsString();
void   ParseBacklogString(const string& backlog, string& base, std::vector<string>& r);

#else

	#define FlushNetLog(...)
	#define InitPrimaryThread(...)
	#define NetLog(...)
	#define VNetLog(...)
	#define NetLogHUD(...)
	#define NetLogAlways(...)
	#define NetLogAlways_Secret(...)
	#define NetError(...)
	#define NetWarning(...)
	#define NetQuickLog(...)
	#define NetPerformanceWarning(...)
	#define NetComment(...)

	#define NetLogPacketDebug(...)

	#define ParseBacklogString(...)

#endif // !defined(EXCLUDE_NORMAL_LOG)

#ifndef _DEBUG
	#define ASSERT_PRIMARY_THREAD
#else
void AssertPrimaryThread();
	#define ASSERT_PRIMARY_THREAD AssertPrimaryThread()
#endif

#endif
