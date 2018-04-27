// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <UnitTest.h>
#include <CryThreading/IJobManager_JobDelegator.h>
#include <Timer.h>

class TestJobHost
{
public:
	volatile bool done = false;
	int value = 0;
	void JobEntry(int x)
	{
		value = x;
		CrySleep(1);
		done = true;
	}
};

DECLARE_JOB("TestJob", TTestJob, TestJobHost::JobEntry);

TEST(JobSystem, All)
{
	gEnv->pJobManager = GetJobManagerInterface();
	gEnv->pTimer = new CTimer();

	JobManager::SJobState jobState;

	std::vector<std::unique_ptr<TestJobHost>> jobObjects;
	for (int i = 0; i < 100; i++)
	{
		std::unique_ptr<TestJobHost> host = stl::make_unique<TestJobHost>();
		TTestJob job(i);
		job.SetClassInstance(host.get());
		job.RegisterJobState(&jobState);
		job.SetPriorityLevel(JobManager::eStreamPriority);
		job.Run();
		jobObjects.push_back(std::move(host));
	}

	jobState.Wait();
	for (int i = 0; i < 100; i++)
	{
		REQUIRE(jobObjects[i]->value == i);
	}

	delete gEnv->pTimer;
}
