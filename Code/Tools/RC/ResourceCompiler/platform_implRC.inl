// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

// This file should only be included Once in DLL module.

#pragma once

#include <CryCore/Platform/platform.h>

#if defined(_LIB)
#	error It is not allowed to have _LIB defined
#endif

#include <CryCore/TypeInfo_impl.h>
#define CRY_PLATFORM_IMPL_H_FILE 1
#include <CryCore/CryTypeInfo.inl>
#undef CRY_PLATFORM_IMPL_H_FILE

#include <IRCLog.h>
#include <CryMath/Random.h>

struct SSystemGlobalEnvironment;
SSystemGlobalEnvironment* gEnv = 0;
IRCLog* g_pRCLog = 0;


void SetRCLog(IRCLog* pRCLog)
{
	g_pRCLog = pRCLog;
}

void RCLog(const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	if (g_pRCLog)
	{
		g_pRCLog->LogV(IRCLog::eType_Info, szFormat, args);
	}
	else
	{
		vprintf(szFormat, args);
		printf("\n");
		fflush(stdout);
	}
	va_end(args);
}

void RCLogWarning(const char* szFormat, ...)
{
	va_list args;
	va_start(args, szFormat);
	if (g_pRCLog)
	{
		g_pRCLog->LogV(IRCLog::eType_Warning, szFormat, args);
	}
	else
	{
		vprintf(szFormat, args);
		printf("\n");
		fflush(stdout);
	}
	va_end(args);
}

void RCLogError(const char* szFormat, ...)
{
	va_list args;
	va_start (args, szFormat);
	if (g_pRCLog)
	{
		g_pRCLog->LogV(IRCLog::eType_Error, szFormat, args);
	}
	else
	{
		vprintf(szFormat, args);
		printf("\n");
		fflush(stdout);
	}
	va_end(args);
}


//////////////////////////////////////////////////////////////////////////
// Log important data that must be printed regardless verbosity.

void PlatformLogOutput(const char *, ...) PRINTF_PARAMS(1, 2);

inline void PlatformLogOutput( const char *format,... )
{
	assert(g_pRCLog);
	if (g_pRCLog)		
	{
		va_list args;
		va_start(args,format);
		g_pRCLog->LogV(IRCLog::eType_Error,  format, args );
		va_end(args);
	}
}
