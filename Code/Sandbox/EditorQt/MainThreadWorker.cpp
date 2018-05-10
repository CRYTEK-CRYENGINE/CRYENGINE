// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MainThreadWorker.h"

namespace Private_MainThreadWorker
{

inline bool IsMainThread()
{
	return gEnv->mMainThreadId == CryGetCurrentThreadId();
}

} // namespace Private_MainThreadWorker

CMainThreadWorker& CMainThreadWorker::GetInstance()
{
	CRY_ASSERT(s_pTheInstance);
	return *s_pTheInstance;
}

CMainThreadWorker::CMainThreadWorker()
{
	CRY_ASSERT(!s_pTheInstance);
	s_pTheInstance = this;
}

void CMainThreadWorker::AddTask(std::function<void()> task)
{
	using namespace Private_MainThreadWorker;

	if (IsMainThread())
	{
		task();
	}
	else
	{
		m_tasks.push(task);
	}
}

bool CMainThreadWorker::TryExecuteNextTask()
{
	std::function<void()> task;
	if (m_tasks.try_pop(task))
	{
		task();
		return true;
	}
	return false;
}

CMainThreadWorker* CMainThreadWorker::s_pTheInstance = nullptr;

