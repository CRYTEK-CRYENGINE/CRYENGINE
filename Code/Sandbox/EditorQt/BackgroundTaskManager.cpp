// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BackgroundTaskManager.h"

namespace BackgroundTaskManager
{

//-----------------------------------------------------------------------------

CTaskManager::CThread::CThread(class CTaskManager* pManager, CQueue* pQueue)
	: m_pManager(pManager)
	, m_pQueue(pQueue)
	, m_bQuit(false)
{
}

CTaskManager::CThread::~CThread()
{
	SignalStopWork();
	gEnv->pThreadManager->JoinThread(this, eJM_Join);
}

void CTaskManager::CThread::SignalStopWork()
{
	m_bQuit = true;
}

void CTaskManager::CThread::ThreadEntry()
{
	while (!m_bQuit && !m_pManager->IsStopped())
	{
		STaskHandle taskHandle;

		// This blocks on Semaphore waiting for task from queue
		m_pQueue->PopTask(taskHandle);

		// Should not happen but it's a stupid way to crash :)
		if (NULL == taskHandle.pTask)
		{
			continue;
		}

		if (taskHandle.pTask->IsCanceled())
		{
			// Task was canceled before we got here
			m_pManager->AddCompletedTask(taskHandle, eTaskResult_Canceled);
		}
		else
		{
			const ETaskResult state = taskHandle.pTask->Work();

			if (state == eTaskResult_Resume)
			{
				// Put it back into queue, so more important task can take over.
				m_pManager->AddTask(taskHandle);
			}
			else
			{
				// Finish task
				m_pManager->AddCompletedTask(taskHandle, state);
			}
		}
	}
}

//-----------------------------------------------------------------------------

CTaskManager::CQueue::CQueue()
	: m_semaphore(INT_MAX, 0) // no good maximum value, assume worst case
{
}

void CTaskManager::CQueue::AddTask(const STaskHandle& taskHandle)
{
	{
		CryAutoLock<CryMutex> lock(m_lock);

		// TODO: use heap?
		m_pendingTasks.insert(m_pendingTasks.begin(), taskHandle);
		std::stable_sort(m_pendingTasks.begin(), m_pendingTasks.end());

		taskHandle.pTask->SetState(eTaskState_Pending);
	}

	// release internal semaphore so threads can pick up the work
	m_semaphore.Release();
}

void CTaskManager::CQueue::PopTask(STaskHandle& outTaskHandle)
{
	// wait for job
	m_semaphore.Acquire();

	{
		CryAutoLock<CryMutex> lock(m_lock);

		if (m_pendingTasks.empty())
		{
			outTaskHandle.pTask = NULL;
		}
		else
		{
			outTaskHandle = m_pendingTasks.back();
			outTaskHandle.pTask->SetState(eTaskState_Working);
			m_pendingTasks.pop_back();
		}
	}
}

void CTaskManager::CQueue::ReleaseSemaphore()
{
	m_semaphore.Release();
}

void CTaskManager::CQueue::Clear()
{
	CryAutoLock<CryMutex> lock(m_lock);
	for (uint i = 0; i < m_pendingTasks.size(); ++i)
	{
		m_pendingTasks[i].pTask->Release();
	}

	m_pendingTasks.clear();
}

//-----------------------------------------------------------------------------

CTaskManager::CTaskManager()
	: m_bStop(false)
	, m_nextTaskID(1)
	, m_listeners(1)
{
	GetIEditorImpl()->RegisterNotifyListener(this);
}

CTaskManager::~CTaskManager()
{
	if (!m_bStop)
	{
		Stop();
	}
}

void CTaskManager::Start(const uint32 threadCount /*=kDefaultThreadCount*/)
{
	m_bStop = false;

	if (m_pThreads.empty())
	{
		// Always create one IO thread
		{
			CThread* pThread = new CThread(this, &m_pendingTasks[eTaskThreadMask_IO]);
			if (!gEnv->pThreadManager->SpawnThread(pThread, "BackgroundTask_IO"))
			{
				CryFatalError("Error spawning \"BackgroundTask_IO\" thread.");
			}
			m_pThreads.push_back(pThread);
		}

		// We also need at least one generic thread
		const uint32 numGenericThreads = max<uint32>(threadCount, 1);
		for (uint32 i = 0; i < numGenericThreads; ++i)
		{
			CThread* pThread = new CThread(this, &m_pendingTasks[eTaskThreadMask_Any]);

			if (!gEnv->pThreadManager->SpawnThread(pThread, "BackgroundTask_%u", i))
			{
				CryFatalError("Error spawning \"BackgroundTask_%u\" thread.", i);
			}
			m_pThreads.push_back(pThread);
		}
	}
}

void CTaskManager::StartScheduledTasks()
{
	CryAutoLock<CryMutex> lock(m_tasksLock);

	if (!m_scheduledTasks.empty())
	{
		const unsigned int time = GetTickCount();
		while (!m_scheduledTasks.empty())
		{
			const int delta = (int)(time - m_scheduledTasks[0].time);
			if (delta > 0)
			{
				// the soonest task on the list is still in the future, no point in looking at the next entries in the list
				break;
			}

			// promote the scheduled task to be a full task
			AddTask(m_scheduledTasks[0].handle);

			// We held a reference to the task on list, release it
			m_scheduledTasks[0].handle.pTask->Release();

			m_scheduledTasks.erase(m_scheduledTasks.begin());
		}
	}
}

void CTaskManager::Stop()
{
	if (!m_bStop)
	{
		m_bStop = true;
		GetIEditorImpl()->UnregisterNotifyListener(this);

		// clear queues - no new tasks will be processed
		for (uint32 i = 0; i < eTaskThreadMask_COUNT; ++i)
		{
			m_pendingTasks[i].Clear();
		}

		// kick all the threads to allow them to quit
		for (uint32 j = 0; j < m_pThreads.size(); ++j)
		{
			for (uint32 i = 0; i < eTaskThreadMask_COUNT; ++i)
			{
				m_pendingTasks[i].ReleaseSemaphore();
			}
		}

		// Stop threads
		for (TWorkerThreads::iterator it = m_pThreads.begin();
		     it != m_pThreads.end(); ++it)
		{
			(*it)->SignalStopWork();
			gEnv->pThreadManager->JoinThread((*it), eJM_Join);
			delete *it;
		}

		m_pThreads.clear();
	}
}

void CTaskManager::AddListener(IBackgroundTaskManagerListener* pListener, const char* name)
{
	m_listeners.Add(pListener, name);
}

void CTaskManager::RemoveListener(IBackgroundTaskManagerListener* pListener)
{
	m_listeners.Remove(pListener);
}

void CTaskManager::AddTask(IBackgroundTask* pTask, ETaskPriority priority, ETaskThreadMask threadMask)
{
	MAKE_SURE(pTask != 0, return );

	// keep an extra reference to the task in the manager
	pTask->AddRef();

	STaskHandle handle;
	handle.id = CryInterlockedIncrement(&m_nextTaskID);
	handle.priority = priority;
	handle.threadMask = threadMask;
	handle.pTask = pTask;

	AddTask(handle);

	for (TListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnBackgroundTaskAdded(pTask->Description());
	}
}

void CTaskManager::ScheduleTask(IBackgroundTask* pTask, ETaskPriority priority, int delayMilliseconds, ETaskThreadMask threadMask)
{
	MAKE_SURE(delayMilliseconds >= 0, return );
	MAKE_SURE(pTask != 0, return );

	// keep an extra reference to the task in the manager
	pTask->AddRef();

	SScheduledTask task;
	task.time = GetTickCount() + delayMilliseconds;
	task.handle.pTask = pTask;
	task.handle.id = CryInterlockedIncrement(&m_nextTaskID);
	task.handle.threadMask = threadMask;
	task.handle.priority = priority;

	{
		CryAutoLock<CryMutex> lock(m_tasksLock);
		m_scheduledTasks.push_back(task);
	}

	for (TListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
	{
		notifier->OnBackgroundTaskAdded(pTask->Description());
	}
}

void CTaskManager::AddTask(const STaskHandle& handle)
{
	MAKE_SURE(handle.pTask != 0, return );
	MAKE_SURE(handle.id != 0, return );

	// add task to appropriate queue (every thread mask has it's own queue)
	m_pendingTasks[handle.threadMask].AddTask(handle);
}

void CTaskManager::AddCompletedTask(const STaskHandle& handle, ETaskResult resultState)
{
	CryAutoLock<CryMutex> lock(m_tasksLock);

	CRY_ASSERT(handle.pTask->GetState() == eTaskState_Working);
	CRY_ASSERT(resultState != eTaskResult_Resume);

	// Update task state
	switch (resultState)
	{
	case eTaskResult_Canceled:
		{
			handle.pTask->SetState(eTaskState_Canceled);
			break;
		}

	case eTaskResult_Completed:
		{
			handle.pTask->SetState(eTaskState_Completed);
			break;
		}

	case eTaskResult_Failed:
		{
			handle.pTask->SetState(eTaskState_Failed);
			break;
		}
	}

	// add to the list of completed tasks (for calling the Finalize)
	// TODO: some of the tasks do not require Finalize() and they could be released here instead of the main thread
	SCompletedTask info;
	info.pTask = handle.pTask;
	info.id = handle.id;
	info.state = resultState;
	m_completedTasks.push_back(info);
}

void CTaskManager::Update()
{
	std::vector<SCompletedTask> completedTasks;
	{
		CryAutoLock<CryMutex> lock(m_tasksLock);
		m_completedTasks.swap(completedTasks);
	}

	// call finalize for the completed tasks
	for (size_t i = 0; i < completedTasks.size(); ++i)
	{
		SCompletedTask& handle = completedTasks[i];

		if (NULL != handle.pTask)
		{
			string description = handle.pTask->Description(); // copy string as the description is used after pTask is destroyed

			if (handle.state == eTaskResult_Completed)
			{
				if (description && description[0] != '\0')
				{
					gEnv->pLog->Log("Task Completed: %s", description.c_str());
				}
			}
			else if (handle.state == eTaskResult_Failed)
			{
				if (description && description[0] != '\0' && !handle.pTask->FailReported())
				{
					gEnv->pLog->LogError("Task Failed: %s ", description.c_str());

					const char* errorMessage = handle.pTask->ErrorMessage();
					if (errorMessage && errorMessage[0] != '\0')
					{
						gEnv->pLog->LogError("\tReason: [%s]", errorMessage);
					}
				}
			}

			handle.pTask->Finalize();

			// release the internal (task manager) reference.
			// Tthis is usually the last reference to the task so it gets deleted here.
			handle.pTask->Release();

			for (TListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
			{
				notifier->OnBackgroundTaskCompleted(handle.state, description.c_str());
			}
		}
	}
}

void CTaskManager::OnEditorNotifyEvent(EEditorNotifyEvent ev)
{
	switch (ev)
	{
	case eNotify_OnInit:
		Start();
		break;
	case eNotify_OnIdleUpdate:
		Update();
		break;
	case eNotify_OnQuit:
		Stop();
		break;
	}
}

}

