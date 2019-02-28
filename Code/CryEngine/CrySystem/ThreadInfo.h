// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryCore/Platform/CryWindows.h>

struct SThreadInfo
{
public:
	struct SThreadHandle
	{
		HANDLE   Handle;
		threadID Id;
	};

	typedef std::vector<threadID>      TThreadIds;
	typedef std::vector<SThreadHandle> TThreads;
	typedef std::map<threadID, string> TThreadInfo;

	// returns thread info
	static void GetCurrentThreads(TThreadInfo& threadsOut);

	// fills threadsOut vector with thread handles of given thread ids; if threadIds vector is emtpy it fills all running threads
	// if ignoreCurrThread is true it will not return the current thread
	static void OpenThreadHandles(TThreads& threadsOut, const TThreadIds& threadIds = TThreadIds(), bool ignoreCurrThread = true);

	// closes thread handles; should be called whenever GetCurrentThreads was called!
	static void CloseThreadHandles(const TThreads& threads);
};
