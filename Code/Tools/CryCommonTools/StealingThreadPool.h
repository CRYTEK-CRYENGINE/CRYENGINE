// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ThreadPool_h__
#define __ThreadPool_h__

#pragma once

#include "ThreadUtils.h"

namespace ThreadUtils {

class StealingWorker;
class JobGroup;

// Simple stealing thread pool
class StealingThreadPool
{
public:
	explicit StealingThreadPool(int numThreads, bool enableTracing = false);
	~StealingThreadPool();

	void Start();
	void WaitAllJobs();
	void WaitAllJobsTemporary();

	const std::vector<JobTraces>& Traces() const{ return m_threadTraces; }
	bool SaveTracesGraph(const char* filename);

	// Submits single independent job
	template<class T>
	void Submit(void(*jobFunc)(T*), T* data)
	{
		Submit(Job((JobFunc)jobFunc, data));
	}

	// Create a group of jobs. A group of jobs can be followed by one "finishing" job. 
	// It is a way to express dependencies between jobs.
	template<class T>
	JobGroup* CreateJobGroup(void(*jobFunc)(T*), T* data)
	{
		return CreateJobGroup((JobFunc)jobFunc, (void*)data);
	}

	uint GetNumThreads() const { return m_numThreads; }

private:
	StealingWorker* FindBestVictim(int exceptFor) const;
	StealingWorker* FindWorstWorker() const;

	void Submit(const Job& job);
	void Submit(const Jobs& jobs);
	JobGroup* CreateJobGroup(JobFunc, void* data);	

	size_t m_numThreads;
	typedef std::vector<class StealingWorker*> ThreadWorkers;
	ThreadWorkers m_workers;

	bool m_enableTracing;
	std::vector<JobTraces> m_threadTraces;	

	volatile long m_numJobsWaitingForExecution;
	volatile long m_numJobs;
	ConditionVariable m_jobsCV;
	ConditionVariable m_jobFinishedCV;

	friend class JobGroup;
	friend class StealingWorker;
};


// JobGroup represents a group of jobs that can be followed by one "finishing"
// job. This is a way to express dependencies between jobs.
class JobGroup
{
public:
	template<class T>
	void Add(void(*jobFunc)(T*), T* data)
	{
		Add((JobFunc)jobFunc, data);
	}

	// Submits group to thread pool
	void Submit();
private:
	struct GroupInfo
	{
		Job m_job;
		JobGroup* m_group;
	};
	typedef std::vector<GroupInfo> GroupInfos;

	JobGroup(StealingThreadPool* pool, JobFunc func, void* data);

	static void Process(JobGroup::GroupInfo* job);
	void Add(JobFunc func, void* data);

	volatile LONG m_numJobsRunning;
	StealingThreadPool* m_pool;
	GroupInfos m_infos;
	Job m_finishJob;
	bool m_submited;
	friend class StealingThreadPool;
};

}

#endif
