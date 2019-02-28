// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ThreadInfo.h"
#include "System.h"
#include <CryThreading/IThreadManager.h>
#include <CryCore/Platform/CryWindows.h>

#if defined(ENABLE_PROFILING_CODE)

////////////////////////////////////////////////////////////////////////////
namespace
{
//////////////////////////////////////////////////////////////////////////
void GetThreadInfo(threadID nThreadId, void* pData)
{
	SThreadInfo::TThreadInfo* threadsOut = (SThreadInfo::TThreadInfo*)pData;
	(*threadsOut)[nThreadId] = gEnv->pThreadManager->GetThreadName(nThreadId);
}
}

////////////////////////////////////////////////////////////////////////////
void SThreadInfo::GetCurrentThreads(TThreadInfo& threadsOut)
{
	GetThreadInfo(GetCurrentThreadId(), &threadsOut);
	gEnv->pThreadManager->ForEachOtherThread(GetThreadInfo, &threadsOut);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
	#if CRY_PLATFORM_WINDOWS || CRY_PLATFORM_DURANGO

////////////////////////////////////////////////////////////////////////////
void SThreadInfo::OpenThreadHandles(TThreads& threadsOut, const TThreadIds& threadIds /* = TThreadIds()*/, bool ignoreCurrThread /* = true*/)
{
	TThreadIds threadids = threadIds;
	if (threadids.empty())
	{
		TThreadInfo threads;
		GetCurrentThreads(threads);
		DWORD currThreadId = GetCurrentThreadId();

		for (TThreadInfo::iterator it = threads.begin(), end = threads.end(); it != end; ++it)
			if (!ignoreCurrThread || it->first != currThreadId)
				threadids.push_back(it->first);
	}
	for (TThreadIds::iterator it = threadids.begin(), end = threadids.end(); it != end; ++it)
	{
		SThreadHandle thread;
		thread.Id = *it;
		thread.Handle = OpenThread(THREAD_ALL_ACCESS, FALSE, *it);
		threadsOut.push_back(thread);
	}
}

////////////////////////////////////////////////////////////////////////////
void SThreadInfo::CloseThreadHandles(const TThreads& threads)
{
	for (TThreads::const_iterator it = threads.begin(), end = threads.end(); it != end; ++it)
		CloseHandle(it->Handle);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
	#elif CRY_PLATFORM_LINUX || CRY_PLATFORM_ORBIS || CRY_PLATFORM_ANDROID || CRY_PLATFORM_APPLE
////////////////////////////////////////////////////////////////////////////
void SThreadInfo::OpenThreadHandles(TThreads& threadsOut, const TThreadIds& threadIds /* = TThreadIds()*/, bool ignoreCurrThread /* = true*/)
{
	assert(false); // not implemented!
}

////////////////////////////////////////////////////////////////////////////
void SThreadInfo::CloseThreadHandles(const TThreads& threads)
{
	assert(false); // not implemented!
}
	#endif
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

#endif //#if defined(ENABLE_PROFILING_CODE)
