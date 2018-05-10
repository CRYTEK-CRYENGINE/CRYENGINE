// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ThreadUtils_h__
#define __ThreadUtils_h__

#pragma once

#include <CryCore/Platform/CryWindows.h>   // CRITICAL_SECTION


namespace ThreadUtils 
{

class CriticalSection
{
	friend class ConditionVariable;

public:
	CriticalSection() 
	{ 
		memset(&m_cs, 0, sizeof(m_cs));
		InitializeCriticalSection(&m_cs); 
	}

	~CriticalSection() 
	{ 
		DeleteCriticalSection(&m_cs); 
	}

	void Lock() 
	{ 
		EnterCriticalSection(&m_cs); 
	}
	void Unlock() 
	{ 
		LeaveCriticalSection(&m_cs); 
	}
	bool TryLock() 
	{ 
		return TryEnterCriticalSection(&m_cs) != FALSE; 
	}

	/*
#ifndef NDEBUG
	bool IsLocked() 
	{ 
		return m_cs.RecursionCount > 0 && (DWORD)m_cs.OwningThread == GetCurrentThreadId(); 
	}
#endif
	*/

private:
	// You are not allowed to copy or move a CRITICAL_SECTION 
	// handle, so make this class non-copyable
	CriticalSection(const CriticalSection &cs);
	CriticalSection &operator=(const CriticalSection &cs);

	CRITICAL_SECTION m_cs;
};

class ConditionVariable
{
public:
	ConditionVariable()
	{
		InitializeConditionVariable(&m_cv);
	}

	void Wake()
	{
		WakeConditionVariable(&m_cv);
	}

	void WakeAll()
	{
		WakeAllConditionVariable(&m_cv);
	}

	void Sleep(CriticalSection &cs, DWORD milliseconds = INFINITE)
	{
		SleepConditionVariableCS(&m_cv, &cs.m_cs, milliseconds);
	}

private:
	// You are not allowed to copy or move a CONDITION_VARIABLE 
	// handle, so make this class non-copyable
	ConditionVariable(const ConditionVariable &cs);
	ConditionVariable &operator=(const ConditionVariable &cs);

	CONDITION_VARIABLE m_cv;
};

class AutoLock
{
private:
	CriticalSection &m_lock;

	AutoLock();
	AutoLock(const AutoLock&);
	AutoLock& operator = (const AutoLock&);

public:
	explicit AutoLock(CriticalSection &lock) 
		: m_lock(lock) 
	{ 
		m_lock.Lock(); 
	}
	~AutoLock() 
	{ 
		m_lock.Unlock(); 
	}
};


typedef void(*JobFunc)(void*);

struct Job
{
	JobFunc m_func;
	void* m_data;
	int m_debugInitialThread;

	Job()
	: m_func(0)
	, m_data(0)
	, m_debugInitialThread(0)
	{
	}

	Job(JobFunc func, void* data)
	: m_func(func)
	, m_data(data)
	, m_debugInitialThread(0)
	{
	}

	void Run()
	{
		m_func(m_data);
	}
};
typedef std::deque<Job> Jobs;

struct JobTrace
{
	Job m_job;
	bool m_stolen;
	int m_duration;

	JobTrace()
	: m_duration(0)
	, m_stolen(false)
	{
	}
};
typedef std::vector<JobTrace> JobTraces;

class SimpleWorker;

class SimpleThreadPool
{
public:
	explicit SimpleThreadPool(bool trace);
	~SimpleThreadPool();

	bool GetJob(Job& job, int threadIndex);

	// Submits single independent job
	template<class T>
	void Submit(void(*jobFunc)(T*), T* data)
	{
		Submit(Job((JobFunc)jobFunc, data));
	}

	void Start(int numThreads);
	void WaitAllJobs();

private:
	void Submit(const Job& job);

	bool m_started;
	bool m_trace;

	std::vector<SimpleWorker*> m_workers;

	std::vector<JobTraces> m_threadTraces;

	int m_numProcessedJobs;
	CriticalSection m_lockJobs;
	std::vector<Job> m_jobs;
};

#pragma pack(push, 8)
struct ThreadNameInfo
{
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
};
#pragma pack(pop)

// Usage: SetThreadName (-1, "MainThread");
// From http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
inline void SetThreadName(DWORD dwThreadID, const char *threadName)
{
#ifdef _DEBUG
	ThreadNameInfo info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;

	__try
	{
		const DWORD exceptionCode = 0x406D1388;
		RaiseException(exceptionCode, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
#endif
}

}

#endif
