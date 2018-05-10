// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ThreadUtils.h"

namespace ThreadUtils {

class SimpleWorker
{
public:
	SimpleWorker(SimpleThreadPool* pool, int index, bool trace)
	: m_pool(pool)
	, m_index(index)
	, m_trace(trace)
	{
		m_eventWork = CreateEvent(0, FALSE, FALSE, 0);
		m_eventFinish = CreateEvent(0, FALSE, FALSE, 0);
	}

	void Start(int startTime)
	{
		m_lastStartTime = startTime;
		m_handle = (HANDLE)_beginthreadex(0, 0, &SimpleWorker::ThreadFunc, (void*)this, 0, 0);
	}

	static unsigned int __stdcall ThreadFunc(void* param)
	{
		SimpleWorker* self = static_cast<SimpleWorker*>(param);
		self->Work();
		return 0;
	}

	void ExecuteJob(Job& job)
	{
		job.Run();
		if (m_trace)
		{
			int time = (int)GetTickCount();

			JobTrace trace;
			trace.m_job = job;
			trace.m_duration = time - m_lastStartTime;
			m_traces.push_back(trace);

			m_lastStartTime = time;
		}
	}

	void Work()
	{
		Job job;
		for (;;)
		{
			if (m_pool->GetJob(job, m_index))
			{
				ExecuteJob(job);
			}
			else 
			{
				return;
			}
		}
	}

	// Called from main thread
	void Join(JobTraces& traces)
	{
		WaitForSingleObject(m_handle, INFINITE);
		CloseHandle(m_handle);
		CloseHandle(m_eventWork);
		CloseHandle(m_eventFinish);

		if (m_trace)
		{
			m_traces.swap(traces);
		}
	}

private:
	HANDLE m_eventWork;
	HANDLE m_eventFinish;

	SimpleThreadPool* m_pool;
	HANDLE m_handle;
	int m_index;
	bool m_trace;
	int m_lastStartTime;
	JobTraces m_traces;
	friend SimpleThreadPool;
};

// ---------------------------------------------------------------------------

SimpleThreadPool::SimpleThreadPool(bool trace)
: m_trace(trace)
, m_started(false)
, m_numProcessedJobs(0)
{
}

SimpleThreadPool::~SimpleThreadPool()
{
	WaitAllJobs();
}

void SimpleThreadPool::Start(int numThreads)
{
	m_workers.resize(numThreads);
	for (int i = 0; i < numThreads; ++i)
	{
		m_workers[i] = new SimpleWorker(this, i, m_trace);
	}

	m_started = true;

	int startTime = (int)GetTickCount();
	for (int i = 0; i < numThreads; ++i)
	{
		m_workers[i]->Start(startTime);
	}
}


void SimpleThreadPool::WaitAllJobs()
{
	size_t numThreads = m_workers.size();
	m_threadTraces.resize(numThreads);
	for (size_t i = 0; i < numThreads; ++i)
	{
		m_workers[i]->Join(m_threadTraces[i]);
	}
	
	for (size_t i = 0; i < numThreads; ++i)
	{
		delete m_workers[i];
	}
	m_workers.clear();

	m_started = false;
}

void SimpleThreadPool::Submit(const Job& job)
{
	assert(!m_started);
	m_jobs.push_back(job);
}

bool SimpleThreadPool::GetJob(Job& job, int threadIndex)
{
	AutoLock lock(m_lockJobs);

	if (m_numProcessedJobs >= m_jobs.size())
		return false;

	job = m_jobs[m_numProcessedJobs];
	++m_numProcessedJobs;
	return true;
}

}
