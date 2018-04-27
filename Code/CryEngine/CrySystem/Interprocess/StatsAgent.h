// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class ICmdLineArg;

//! Provides utilities for inter-process communication using pipe
class CStatsAgent
{
public:
	static void CreatePipe(const ICmdLineArg* pPipeName);
	static void ClosePipe(void);
	static void Update(void);

	static void SetDelayMessages(bool enable);
	static void SetDelayTimeout(const int timeout);

	CStatsAgent() = delete;
};
