// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//! Wrapper around windows pipe. XB1 (Durango) in theory is supported, but this needs verification.
class CStatsAgentPipe
{
public:
	static bool OpenPipe(const char *szPipeName);
	static void ClosePipe();

	static bool Send(const char *szMessage, const char *szPrefix = 0, const char *szDebugTag = 0);
	static const char* Receive();

	static bool IsPipeOpen();

private:
	CStatsAgentPipe(); // prevent instantiation

	static bool CreatePipe(const char *szName);
};
