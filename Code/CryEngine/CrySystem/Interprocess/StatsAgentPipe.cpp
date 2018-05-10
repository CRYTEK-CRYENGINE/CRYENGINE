// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#if defined(ENABLE_STATS_AGENT)

#include <CryCore/Project/ProjectDefines.h>
#include "StatsAgentPipe.h"
#include <CryCore/Platform/CryWindows.h>

namespace
{
	const int BUFFER_SIZE = 1024;

	CryFixedStringT<BUFFER_SIZE> s_pCommandBuffer;
	bool s_pipeOpen = false;

#if CRY_PLATFORM_DURANGO //FIXME ?
	constexpr char kPipeBaseName[] = "";
	volatile bool s_commandWaiting = false;
#elif CRY_PLATFORM_WINDOWS
	constexpr char kPipeBaseName[] = R"(\\.\pipe\CrysisTargetComms)";
	HANDLE s_pipe = INVALID_HANDLE_VALUE;
#endif
};

static int statsagent_debug = 0;

bool CStatsAgentPipe::IsPipeOpen()
{
	return s_pipeOpen;
}

bool CStatsAgentPipe::OpenPipe(const char *szPipeName)
{
	REGISTER_CVAR(statsagent_debug, 0, 0, "Enable/Disable StatsAgent debug messages");

	// Construct the pipe name
	string sPipeFullName;
	sPipeFullName.Format("%s%s.pipe", kPipeBaseName, szPipeName);

	CreatePipe(sPipeFullName.c_str());

	if (statsagent_debug && s_pipeOpen)
	{
		CryLogAlways("CStatsAgent: Pipe connection \"%s\" is open", sPipeFullName.c_str());
	}

	if (s_pipeOpen)
	{
		if (!Send("connected"))
			ClosePipe();
	}
	else
	{
		if (statsagent_debug)
			CryLogAlways("CStatsAgent: Unable to connect pipe %s", sPipeFullName.c_str());
	}
	return s_pipeOpen;
}

bool CStatsAgentPipe::CreatePipe(const char *szName)
{
#if CRY_PLATFORM_WINDOWS
	s_pipe = ::CreateFile(szName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
	DWORD dwMode = PIPE_NOWAIT;
	if (s_pipe != INVALID_HANDLE_VALUE)
	{
		CryLogAlways("Pipe %s created", szName);
		s_pipeOpen = ::SetNamedPipeHandleState(s_pipe, &dwMode, NULL, NULL) == TRUE;
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "Failed to create pipe %s, error: 0x%X", szName, GetLastError());
	}
#endif

	return s_pipeOpen;
}

void CStatsAgentPipe::ClosePipe()
{
	if (s_pipeOpen)
	{
#if CRY_PLATFORM_WINDOWS 
		::CloseHandle(s_pipe);
		s_pipe = INVALID_HANDLE_VALUE;
#endif
		s_pipeOpen = false;
	}
}

bool CStatsAgentPipe::Send(const char *szMessage, const char *szPrefix, const char* szDebugTag)
{
	CryFixedStringT<BUFFER_SIZE> pBuffer;
	if (szPrefix)
	{
		pBuffer = szPrefix;
		pBuffer.Trim();
		pBuffer += " ";
	}
	pBuffer += szMessage;

	bool bSuccess = true;
	uint32 nBytes = pBuffer.size() + 1;

	if (statsagent_debug)
	{
		if (szDebugTag)
		{
			CryLogAlways("CStatsAgent: Sending message \"%s\" [%s]", pBuffer.c_str(), szDebugTag);
		}
		else
		{
			CryLogAlways("CStatsAgent: Sending message \"%s\"", pBuffer.c_str());
		}
	}

#if CRY_PLATFORM_WINDOWS
	DWORD tx;
	bSuccess = ::WriteFile(s_pipe, pBuffer.c_str(), nBytes, &tx, 0) == TRUE;
#endif

	if (statsagent_debug && !bSuccess)
		CryLogAlways("CStatsAgent: Unable to write to pipe");

	return bSuccess;
}

const char* CStatsAgentPipe::Receive()
{
	const char *szResult = NULL;

#if CRY_PLATFORM_WINDOWS
	DWORD size;
	if (::ReadFile(s_pipe, s_pCommandBuffer.m_strBuf, BUFFER_SIZE - 1, &size, 0) && size > 0)
	{
		s_pCommandBuffer.m_strBuf[size] = '\0';
		s_pCommandBuffer.TrimRight('\n');
		szResult = s_pCommandBuffer.c_str();
	}
#endif

	if (statsagent_debug && szResult)
		CryLogAlways("CStatsAgent: Received message \"%s\"", szResult);

	return szResult;
}

#endif	// defined(ENABLE_STATS_AGENT)
