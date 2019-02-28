// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NetLog.h"
#include "Network.h"
#include <CryMemory/STLPoolAllocator.h>
#include <CrySystem/ITextModeConsole.h>
#include <CryRenderer/IRenderAuxGeom.h>

threadID g_primaryThread;

#if !defined(EXCLUDE_NORMAL_LOG)

//#define NET_USE_EXTERNAL_LOG
	#if defined(NET_USE_EXTERNAL_LOG)
FILE* g_pExternalLog = fopen("NetLog.log", "w");
	#endif // defined(NET_USE_EXTERNAL_LOG)

enum ENetLogSeverity
{
	eNLS_Error = 1,
	eNLS_Warning,
	eNLS_Performance,
	eNLS_Information,
	eNLS_Comment,
	eNLS_Always,
	eNLS_OnTheMainScreen = 0x80000000,
	eNLS_Secret          = 0x40000000,
};

static int g_tempVarY = 0;

struct SNetLogRecord
{
	uint32              sev;
	static const size_t STRUCT_SIZE = 1024;
	static const size_t MSG_SIZE = STRUCT_SIZE - sizeof(ENetLogSeverity);
	float               timeout;
	float               lifetime;
	char                msg[MSG_SIZE];
	bool                seen;
	char                tstamp[32];

	SNetLogRecord()
	{
		++g_objcnt.netLogRec;
	}

	~SNetLogRecord()
	{
		--g_objcnt.netLogRec;
	}

	SNetLogRecord(const SNetLogRecord& rhs)
	{
		sev = rhs.sev;
		timeout = rhs.timeout;
		lifetime = rhs.lifetime;
		memcpy(msg, rhs.msg, MSG_SIZE);
		seen = rhs.seen;
		memcpy(tstamp, rhs.tstamp, 32);

		++g_objcnt.netLogRec;
	}

	SNetLogRecord& operator=(const SNetLogRecord& rhs)
	{
		sev = rhs.sev;
		timeout = rhs.timeout;
		lifetime = rhs.lifetime;
		memcpy(msg, rhs.msg, MSG_SIZE);
		seen = rhs.seen;
		memcpy(tstamp, rhs.tstamp, 32);
		return *this;
	}

	bool Print(int phase)
	{
		static float clr[3][4] = {
			{ 1, 1, 1, 1 },
			{ 0, 1, 0, 1 },
			{ 1, 1, 0, 1 }
		};
		if (!seen)
		{
			switch (sev & 0xfffffff)
			{
			case eNLS_Always:
	#if defined(NET_USE_EXTERNAL_LOG)
				fprintf(g_pExternalLog, "[net %s] %s\n", tstamp, msg);
				fflush(g_pExternalLog);
	#else
				CryLogAlways("[net %s] %s", tstamp, msg);
	#endif // defined(NET_USE_EXTERNAL_LOG)
				break;
			case eNLS_Comment:
	#if defined(NET_USE_EXTERNAL_LOG)
				fprintf(g_pExternalLog, "[net %s] %s\n", tstamp, msg);
				fflush(g_pExternalLog);
	#else
				CryComment("[net %s] %s", tstamp, msg);
	#endif // defined(NET_USE_EXTERNAL_LOG)
				break;
			case eNLS_Information:
			case eNLS_Performance:
	#if defined(NET_USE_EXTERNAL_LOG)
				fprintf(g_pExternalLog, "[net %s] %s\n", tstamp, msg);
				fflush(g_pExternalLog);
	#else
				CryLog("[net %s] %s", tstamp, msg);
	#endif // defined(NET_USE_EXTERNAL_LOG)
				break;
			case eNLS_Error:
	#if defined(NET_USE_EXTERNAL_LOG)
				fprintf(g_pExternalLog, "[net %s] %s\n", tstamp, msg);
				fflush(g_pExternalLog);
	#else
				CryFatalError("$4[net %s] %s", tstamp, msg);
	#endif // defined(NET_USE_EXTERNAL_LOG)
				break;
			case eNLS_Warning:
	#if defined(NET_USE_EXTERNAL_LOG)
				fprintf(g_pExternalLog, "[net %s] %s\n", tstamp, msg);
				fflush(g_pExternalLog);
	#else
				CryWarning(VALIDATOR_MODULE_NETWORK, VALIDATOR_WARNING, "$6[net %s] %s", tstamp, msg);
	#endif // defined(NET_USE_EXTERNAL_LOG)
				break;
			}
		}
		bool done = true;
		if (sev & eNLS_OnTheMainScreen)
		{
			if (seen && phase != 1)
				done = false;
			else
			{
				float maxTime = timeout;
				if (maxTime < 0.1f)
					maxTime = 0.1f;
				float alpha = 1.0f - (lifetime / maxTime);
				alpha = CLAMP(alpha, 0, 1);
				lifetime += gEnv->pTimer->GetFrameTime();
				clr[phase][3] = alpha;
				gEnv->pAuxGeomRenderer->Draw2dLabel(10.f, (float)(g_tempVarY += 10), 1, clr[phase], false, "%s", msg);
				clr[phase][3] = 1.0f;
				done = lifetime >= timeout;

				if (ITextModeConsole* pTMC = gEnv->pSystem->GetITextModeConsole())
					pTMC->PutText(0, g_tempVarY / 10, msg);
			}
		}
		seen = true;
		return done;
	}

	bool operator<(const SNetLogRecord& r) const
	{
		return seen < r.seen;
	}
};

typedef std::list<SNetLogRecord, stl::STLPoolAllocator<SNetLogRecord, stl::PoolAllocatorSynchronizationSinglethreaded>> RecordList;
static RecordList* records;

void InitPrimaryThread()
{
	g_primaryThread = GetCurrentThreadId();
}

void AssertPrimaryThread()
{
	NET_ASSERT(IsPrimaryThread() || CNetwork::Get()->IsMinimalUpdate());
}

void FlushNetLog(bool final)
{
	if ((CNetCVars::Get().LogLevel == 0))
	{
		return;
	}

	SCOPED_GLOBAL_LOG_LOCK;
	if (!records)
		return;

	int phase = final ? 2 : 1;

	if (!final)
		g_tempVarY = 0;

	records->sort();

	int i = 0;
	for (RecordList::iterator iter = records->begin(); iter != records->end(); )
	{
		RecordList::iterator next = iter;
		++next;

		if (iter->Print(phase))
			records->erase(iter);

		i++;
		iter = next;
	}
}

	#define BACKLOG_DELIMITER '\n'

typedef MiniQueue<string, 16> TNetBacklog;
static TNetBacklog g_backlog;

string GetBacklogAsString()
{
	SCOPED_GLOBAL_LOG_LOCK;
	string sr;
	for (TNetBacklog::SIterator it = g_backlog.RBegin(); it != g_backlog.REnd(); --it)
	{
		sr += BACKLOG_DELIMITER;
		sr += *it;
	}
	return sr;
}

void ParseBacklogString(const string& backlog, string& base, std::vector<string>& r)
{
	base = "";
	r.resize(0);

	size_t off = backlog.find(BACKLOG_DELIMITER);
	if (off == string::npos)
	{
		base = backlog;
		return;
	}
	base = backlog.substr(0, off++);

	while (off < backlog.size())
	{
		size_t pos = backlog.find(BACKLOG_DELIMITER, off);
		if (pos == string::npos)
		{
			r.push_back(backlog.substr(off));
			break;
		}
		if (pos > off)
			r.push_back(backlog.substr(off, pos - off));
		off = pos + 1;
	}
}

static void RecordNetLog(uint32 sev, float timeout, const char* fmt, va_list args)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	SNetLogRecord rec;
	rec.sev = sev & ~eNLS_Secret;
	cry_vsprintf(rec.msg, fmt, args);
	rec.msg[SNetLogRecord::MSG_SIZE - 1] = 0;
	rec.seen = false;
	rec.timeout = timeout;
	rec.lifetime = 0.0f;
	cry_strcpy(rec.tstamp, GetTimestampString());

	if ((sev & eNLS_Secret) == 0)
	{
		SCOPED_GLOBAL_LOG_LOCK;
		g_backlog.CyclePush(string().Format("[%s] %s", rec.tstamp, rec.msg));
	}

	if (timeout <= 0.0f && IsPrimaryThread())
		rec.Print(0);
	else
	{
		MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Debug, 0, "Net Log");
		// SCOPED_GLOBAL_LOCK_NO_LOG;
		SCOPED_GLOBAL_LOG_LOCK;
		if (!records)
			records = new RecordList;

		records->push_back(rec);
	}
}

void NetComment(const char* fmt, ...)
{
	#if ENABLE_DEBUG_KIT
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	if (CVARS.LogComments)
	{
		va_list args;
		va_start(args, fmt);
		RecordNetLog(eNLS_Comment, 0.0f, fmt, args);
		va_end(args);
	}
	#endif
}

void NetLog(const char* fmt, ...)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);
	VNetLog(fmt, args);
	va_end(args);
}

void VNetLog(const char* pFmt, va_list args)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	RecordNetLog(eNLS_Information, 0.0f, pFmt, args);
}

void NetLogAlways(const char* fmt, ...)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);
	RecordNetLog(eNLS_Always, 0.0f, fmt, args);
	va_end(args);
}

void NetLogAlways_Secret(const char* fmt, ...)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);
	RecordNetLog(eNLS_Always | eNLS_Secret, 0.0f, fmt, args);
	va_end(args);
}

void NetWarning(const char* fmt, ...)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);
	RecordNetLog(eNLS_Warning, 0.0f, fmt, args);
	va_end(args);
}

void NetError(const char* fmt, ...)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);
	RecordNetLog(eNLS_Error, 0.0f, fmt, args);
	va_end(args);
}

void NetPerformanceWarning(const char* fmt, ...)
{
	#if ENABLE_DEBUG_KIT
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	if (gEnv->IsEditor())
		return;

	if (CVARS.LogComments)
	{
		va_list args;
		va_start(args, fmt);
		RecordNetLog(eNLS_Performance, 0.0f, fmt, args);
		va_end(args);
	}
	#endif
}

void NetLogHUD(const char* fmt, ...)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);
	RecordNetLog(eNLS_OnTheMainScreen, 10.0f, fmt, args);
	va_end(args);
}

void NetQuickLog(bool toConsole, float timeout, const char* fmt, ...)
{
	if (CNetCVars::Get().LogLevel == 0)
	{
		return;
	}

	va_list args;
	va_start(args, fmt);
	RecordNetLog(eNLS_OnTheMainScreen | ((int)toConsole * eNLS_Information), timeout, fmt, args);
	va_end(args);
}

void NET_ASSERT_FAIL(const char* check, const char* file, int line)
{
	#if NET_ASSERT_LOGGING
	if (!CNetCVars::Get().AssertLogging)
		return;

	if (file)
		NetLog("ASSERT FAILED: %s at %s:%d", check, file, line);
	else
		NetLog("ASSERT FAILED: %s", check);

	static const int MAX_CALLSTACK_ENTS = 32;
	const char* ents[MAX_CALLSTACK_ENTS];
	int numEnts = MAX_CALLSTACK_ENTS;
	gEnv->pSystem->debug_GetCallStack(ents, numEnts);
	for (int i = 0; i < numEnts; i++)
		NetLog("               %s", ents[i]);
	#endif
}

	#if ENABLE_CORRUPT_PACKET_DUMP
void NetLogPacketDebug(const char* fmt, ...)
{
	if (CNetCVars::Get().packetReadDebugOutput)
	{
		va_list args;
		va_start(args, fmt);
		RecordNetLog(eNLS_Information, 0.0f, fmt, args);
		va_end(args);
	}
}
	#endif
#endif // !defined(EXCLUDE_NORMAL_LOG)
