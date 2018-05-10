// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryThreading/MultiThread_Containers.h>

#include <functional>

//! CMainThreadWorker stores a list of tasks for execution on the main thread.
class CMainThreadWorker
{
public:
	static CMainThreadWorker& GetInstance();

	CMainThreadWorker();

	void AddTask(std::function<void()> task);
	bool TryExecuteNextTask();

private:
	CryMT::queue<std::function<void()>> m_tasks;

	static CMainThreadWorker* s_pTheInstance;
};

