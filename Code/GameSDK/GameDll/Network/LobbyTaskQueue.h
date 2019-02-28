// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Lobby task queue
							- Ensures only 1 task is in progress at once

-------------------------------------------------------------------------
History:
- 21:06:2010 : Created by Colin Gulliver

*************************************************************************/

#ifndef __LOBBY_TASK_QUEUE_H__
#define __LOBBY_TASK_QUEUE_H__
#pragma once

//---------------------------------------------------------------------------
class CLobbyTaskQueue
{
public:
	enum ESessionTask
	{
		eST_None,
		eST_Create,
		eST_Migrate,
		eST_Join,
		eST_Delete,
		eST_SetLocalUserData,
		eST_SessionStart,
		eST_SessionEnd,
		eST_Query,
		eST_Update,
		eST_EnsureBestHost,
		eST_FindGame,
		eST_TerminateHostHinting,
		eST_SessionUpdateSlotType,
		eST_SessionSetLocalFlags,
		eST_SessionRequestDetailedInfo,
		eST_Unload,
		eST_SetupDedicatedServer,
		eST_ReleaseDedicatedServer,
	};

	typedef void (*TaskStartedCallback)(ESessionTask task, void *pArg);

	CLobbyTaskQueue();

	void Reset();
	void Init(TaskStartedCallback cb, void *pArg);

	void ClearNotStartedTasks();

	void AddTask(ESessionTask task, bool bUnique);
	void RestartTask();
	void TaskFinished();
	void Update();
	void CancelTask(ESessionTask task);

	bool HasTaskInProgress();
	int GetCurrentQueueSize();
	ESessionTask GetCurrentTask();

	void ClearNonVitalTasks();
	void ClearInternalSessionTasks();

private:
	void CancelTaskByIndex(int index);

	static const int MAX_TASKS = 8;

	ESessionTask m_taskQueue[MAX_TASKS];
	int m_numTasks;
	bool m_taskInProgress;

	TaskStartedCallback m_taskStartedCB;
	void *m_pCBArg;
};


//---------------------------------------------------------------------------
inline CLobbyTaskQueue::CLobbyTaskQueue()
{
	m_taskStartedCB = NULL;
	m_pCBArg = NULL;

	Reset();
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::Reset()
{
	for (int i = 0; i < MAX_TASKS; ++ i)
	{
		m_taskQueue[i] = eST_None;
	}
	m_numTasks = 0;
	m_taskInProgress = false;
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::Init(TaskStartedCallback cb, void *pArg)
{
	CRY_ASSERT(m_taskStartedCB == NULL);
	m_taskStartedCB = cb;
	m_pCBArg = pArg;
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::ClearNotStartedTasks()
{
	if (m_taskQueue[0] != eST_None)
	{
		const int startPos = (m_taskInProgress ? 1 : 0);
		for (int i = startPos; i < MAX_TASKS; ++ i)
		{
			m_taskQueue[i] = eST_None;
		}
		m_numTasks = startPos;
	}
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::AddTask(ESessionTask task, bool bUnique)
{
	CRY_ASSERT(m_numTasks < MAX_TASKS);
	if (m_numTasks < MAX_TASKS)
	{
		if (bUnique)
		{
			// If we're adding unique, make sure the task isn't in the queue.
			// Note: If the same task is currently in progress then add it again since the data
			// will probably have changed
			const int startPoint = (m_taskInProgress ? 1 : 0);
			for (int i = startPoint; i < MAX_TASKS; ++ i)
			{
				if (m_taskQueue[i] == task)
				{
					return;
				}
			}
		}

		m_taskQueue[m_numTasks] = task;
		++ m_numTasks;
	}
#ifndef _RELEASE
	else
	{
		CryLog("CLobbyTaskQueue::AddTask() ERROR: run out of space when trying to add task %u", task);
		for (int i = 0; i < MAX_TASKS; ++ i)
		{
			CryLog("  %i: %u", i, m_taskQueue[i]);
		}
	}
#endif
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::RestartTask()
{
	CRY_ASSERT(m_taskInProgress);
	m_taskInProgress = false;
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::TaskFinished()
{
	CRY_ASSERT(m_taskInProgress);
	CRY_ASSERT(m_numTasks > 0);

	if (m_taskInProgress)
	{
		m_taskInProgress = false;
		for (int i = 1; i < MAX_TASKS; ++ i)
		{
			m_taskQueue[i - 1] = m_taskQueue[i];
		}
		m_taskQueue[MAX_TASKS - 1] = eST_None;
		-- m_numTasks;
	}
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::Update()
{
	if ((!m_taskInProgress) && (m_numTasks))
	{
		m_taskInProgress = true;
		CRY_ASSERT(m_taskStartedCB);
		(m_taskStartedCB)(m_taskQueue[0], m_pCBArg);
	}
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::CancelTask(ESessionTask task)
{
	if (m_taskInProgress && (m_taskQueue[0] == task))
	{
		m_taskInProgress = false;
	}

	for (int i = 0; i < MAX_TASKS; ++ i)
	{
		if (m_taskQueue[i] == task)
		{
			CancelTaskByIndex(i);
			break;
		}
	}
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::CancelTaskByIndex(int index)
{
	m_taskQueue[index] = eST_None;
	-- m_numTasks;
	for (int i = (index + 1); i < MAX_TASKS; ++ i)
	{
		m_taskQueue[i - 1] = m_taskQueue[i];
		m_taskQueue[i] = eST_None;
	}
}

//---------------------------------------------------------------------------
inline bool CLobbyTaskQueue::HasTaskInProgress()
{
	return m_taskInProgress;
}

//---------------------------------------------------------------------------
inline int CLobbyTaskQueue::GetCurrentQueueSize()
{
	return m_numTasks;
}

//---------------------------------------------------------------------------
inline CLobbyTaskQueue::ESessionTask CLobbyTaskQueue::GetCurrentTask()
{
	if (m_taskInProgress)
	{
		return m_taskQueue[0];
	}
	else
	{
		return eST_None;
	}
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::ClearNonVitalTasks()
{
	// Cancels all tasks not currently running that arent either SessionEnd or Delete (since these have to be done)
	const int startPos = (m_taskInProgress ? 1 : 0);
	for (int i = startPos; (i < MAX_TASKS) && (m_taskQueue[i] != eST_None); ++ i)
	{
		if ((m_taskQueue[i] != eST_SessionEnd) && (m_taskQueue[i] != eST_Delete))
		{
			CancelTaskByIndex(i);
			-- i;
		}
	}
}

//---------------------------------------------------------------------------
inline void CLobbyTaskQueue::ClearInternalSessionTasks()
{
	// Cancels all tasks that aren't valid unless we're in a session
	const int startPos = (m_taskInProgress ? 1 : 0);
	for (int i = startPos; (i < MAX_TASKS) && (m_taskQueue[i] != eST_None); ++ i)
	{
		ESessionTask task = m_taskQueue[i];
		if ((task != eST_Join) && (task != eST_Create) && (task != eST_FindGame) && (task != eST_Unload))
		{
			CancelTaskByIndex(i);
			-- i;
		}
	}
}

#endif // __LOBBY_TASK_QUEUE_H__
