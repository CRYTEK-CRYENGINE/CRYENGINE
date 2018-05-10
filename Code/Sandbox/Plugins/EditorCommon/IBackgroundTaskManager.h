// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//
// IBackgroundTaskManager runs background tasks in worker thread.
//
// Tasks are derived from IBackgroundTask. Each task is split in two parts for:
//  Work - done in background thread.
//  Finalize - called afterward in main thread to apply results.
//
// Task objects are reference counted. Task manager will hold its own reference to the task object
// for as long as the task is pending or being executed. If you want to keep the task object around
// in your code you will have to call AddRef() and Release() on the task object by yourself so there
// will be an extra reference to the task object held by your code.
//
// Work returns the state of the task. Task can be resumed, then the Work
// method will be called again. Other tasks can work between calls to Work.
// It is possible to Cancel task. Work method is not invoked any more for
// Canceled tasks.
//

enum ETaskPriority
{
	eTaskPriority_FileUpdateFinal,
	eTaskPriority_BackgroundScan,
	eTaskPriority_FileUpdate,
	eTaskPriority_RealtimePreview
};

// Result code returned by task Work() function
enum ETaskResult
{
	// Task has not yet completed, add it back to the task queue with the same parameters (priority and thread mask)
	eTaskResult_Resume,

	// Task has completed without errors (it's assumed that the result the task was supposed to achieve was achieved)
	eTaskResult_Completed,

	// Task was canceled
	eTaskResult_Canceled,

	// Task has failed to complete it's work
	eTaskResult_Failed,
};

// Internal task tracking state
enum ETaskState
{
	// Task was just created.
	eTaskState_Created,

	// Task was scheduled to be executed in the future.
	eTaskState_Scheduled,

	// Task was added to the task queue and is waiting for it's time to be executed.
	eTaskState_Pending,

	// Task is being processed right now.
	eTaskState_Working,

	// Task was canceled before it has finished (indication that TaskManager has seen the Cancel() call).
	eTaskState_Canceled,

	// Task Work() function was called but it ended with an error code.
	eTaskState_Failed,

	// Task has completed it's Work() function without errors.
	eTaskState_Completed,
};

// Thread mask controls which on which threads given task can be executed.
enum ETaskThreadMask
{
	// Task can run only on the IO thread (default)
	// There is only one IO thread so all task with this flag are run in a sequence.
	eTaskThreadMask_IO,

	// Task can run on any thread (concurrent tasks allowed)
	// There can be many threads with this mask so there's no limit on the concurrent task count.
	eTaskThreadMask_Any,

	eTaskThreadMask_COUNT,
};

struct IBackgroundTask
{
public:
	IBackgroundTask()
		: m_bCanceled(false)
		, m_state(eTaskState_Created)
		, m_progress(-1.0f)
		, m_refCount(0)
		, m_bFailReported(false)
	{}

	void Cancel()
	{
		m_bCanceled = true;
	}

	bool IsCanceled() const
	{
		return m_bCanceled;
	}

	bool HasFinished() const
	{
		return (m_state == eTaskState_Canceled) ||
		       (m_state == eTaskState_Completed) ||
		       (m_state == eTaskState_Failed);
	}

	bool HasFinishedWithoutError() const
	{
		return (m_state == eTaskState_Completed);
	}

	ETaskState GetState() const
	{
		return m_state;
	}

	void SetState(ETaskState state)
	{
		m_state = state;
	}

	float GetProgress() const
	{
		return m_progress;
	}

	int AddRef()
	{
		return CryInterlockedIncrement(&m_refCount);
	}

	int Release()
	{
		const int nCount = CryInterlockedDecrement(&m_refCount);
		assert(nCount >= 0);
		if (nCount == 0)
		{
			Delete();
		}
		else if (nCount < 0)
		{
			assert(0);
			CryFatalError("Deleting Reference Counted Object Twice");
		}
		return nCount;
	}

	bool FailReported() const { return m_bFailReported; }

	// Get the user readable description (name) of this task, used for logging
	virtual const char* Description() const { return ""; }

	// Get the user readable error message (in case when the task fails), used for logging the errors
	virtual const char* ErrorMessage() const { return ""; }

	// Called from main thread after task is completed just before the task gets destroyed
	virtual void Finalize() {}

	// Since there's a possibility that task object were created using different allocator
	// we need a way to delete the task object once we are done with it
	virtual void Delete() = 0;

	// Invoked from worker thread, actual work is done here
	virtual ETaskResult Work() = 0;

protected:
	void SetProgress(float progress) { m_progress = progress; }
	void SetFailReported()           { m_bFailReported = true; }

	// destructor is hidden to indicate that we should use Release() method
	virtual ~IBackgroundTask() {}

private:
	volatile int m_refCount;
	ETaskState   m_state;
	float        m_progress;
	bool         m_bCanceled;
	bool         m_bFailReported;
};

struct IBackgroundTaskManagerListener
{
	virtual ~IBackgroundTaskManagerListener() {}

	virtual void OnBackgroundTaskAdded(const char* description) = 0;
	virtual void OnBackgroundTaskCompleted(ETaskResult taskResult, const char* description) = 0;
	virtual void GetStatus(bool& hasFailures, bool& isIdle, string& statusText, string& extendedStatusText) = 0;
};

struct IBackgroundTaskManager
{
	virtual ~IBackgroundTaskManager() {}

	// Add task to the queue with given priority and thread mask. If tasks are of equal priority, it serves the tasks as a FIFO queue.
	virtual void AddTask(IBackgroundTask* pTask, ETaskPriority priority, ETaskThreadMask threadMask) = 0;

	// Schedule task to be executed in the future
	virtual void ScheduleTask(IBackgroundTask* pTask, ETaskPriority priority, int delayMilliseconds, ETaskThreadMask threadMask) = 0;

	virtual void AddListener(IBackgroundTaskManagerListener* pListener, const char* name) = 0;

	virtual void RemoveListener(IBackgroundTaskManagerListener* pListener) = 0;
};

