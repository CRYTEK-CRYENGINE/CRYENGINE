// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved. 

#include <UnitTest.h>
#include <CryThreading/IJobManager.h>
#include <CryThreading/IJobManager_JobDelegator.h>
#include <CryThreading/IThreadManager.h>
#include <Timer.h>
#include <System.h> // CrySystem module's System.h
#include <vector>
#include <memory>
#include <atomic>
#include <thread>

void InitJobManager()
{
	gEnv->pJobManager = GetJobManagerInterface();
	
	gEnv->startProfilingSection = [](SProfilingSection*) { return false; };
	gEnv->recordProfilingMarker = [](SProfilingMarker*) {};
	gEnv->pJobManager->Init(8);
}

void ShutdownJobManager()
{
	gEnv->pJobManager->ShutDown();
}

// Tests that the job manager can be properly initialized and shutdown without crash or hang
TEST(CJobManagerTest, InitShutdown)
{
	gEnv->pTimer = new CTimer();
	if (!gEnv->pThreadManager)
		gEnv->pThreadManager = CreateThreadManager();

	for (int i = 0; i < 100; ++i)
	{	
		InitJobManager();
		ShutdownJobManager();
	}

	SAFE_DELETE(gEnv->pTimer);
}

class CTestJobHost
{
public:
	void JobEntry(int x)
	{
		m_value = x;
		CrySleep(1);
		m_isDone = true;
	}

	int GetValue() const { return m_value; }
	bool IsDoneCalculating() const { return m_isDone; }

private:
	volatile bool m_isDone = false;
	int m_value = 0;
};

DECLARE_JOB("TestJob", TTestJob, CTestJobHost::JobEntry);

class CJobSystemTest : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
		gEnv->pTimer = new CTimer();
		if (!gEnv->pThreadManager)
			gEnv->pThreadManager = CreateThreadManager();
		InitJobManager();
	}

	virtual void TearDown() override
	{
		ShutdownJobManager();
		SAFE_DELETE(gEnv->pTimer);
	}
};

TEST_F(CJobSystemTest, MemberFunctionJobSimple)
{
	CTestJobHost host;
	TTestJob job(42);
	job.SetClassInstance(&host);
	job.Run(JobManager::eStreamPriority);
	while (!host.IsDoneCalculating()) {}
	REQUIRE(host.GetValue() == 42);
}

TEST_F(CJobSystemTest, MemberFunctionJobMultiple)
{
	JobManager::SJobState jobState;

	std::vector<std::unique_ptr<CTestJobHost>> jobObjects;
	for (int i = 0; i < 100; i++)
	{
		std::unique_ptr<CTestJobHost> host = stl::make_unique<CTestJobHost>();
		TTestJob job(i);
		job.SetClassInstance(host.get());
		job.Run(JobManager::eStreamPriority, &jobState);
		jobObjects.push_back(std::move(host));
	}

	jobState.Wait();
	for (int i = 0; i < 100; i++)
	{
		REQUIRE(jobObjects[i]->GetValue() == i);
	}
}

// Tests whether the job system is able to cache the copy of parameters passed to the job on creation.
// The job system must not directly forward l-value references to the callback, it must store a copy.
TEST_F(CJobSystemTest, MemberFunctionJobLifeTime)
{
	struct SLifeTimeTestHost
	{
		void JobEntry(const string& str)
		{
			m_value = str;
			m_isDone = true;
		}
		volatile bool m_isDone = false;
		string m_value;
	};

	static SLifeTimeTestHost gJobSystemLifeTimeTestHost;

	DECLARE_JOB("SLifeTimeTestHost", SLifeTimeTestHostJob, SLifeTimeTestHost::JobEntry);

	auto GetLifeTimeTestHostJob = []
	{
		string str = "abc";
		SLifeTimeTestHostJob* pJob = new SLifeTimeTestHostJob(str);
		return pJob;
	};

	gJobSystemLifeTimeTestHost = {}; //reset

	SLifeTimeTestHostJob* pJob = GetLifeTimeTestHostJob();
	pJob->SetClassInstance(&gJobSystemLifeTimeTestHost);
	pJob->Run();
	while (!gJobSystemLifeTimeTestHost.m_isDone) {}
	REQUIRE(gJobSystemLifeTimeTestHost.m_value == "abc");
	delete pJob;
}


static string gFreeFunctionLifetimeTestStringResult;

void SFreeFunctionLifeTimeTestCallback(const string& str)
{
	gFreeFunctionLifetimeTestStringResult = str;
}

DECLARE_JOB("SFreeFunctionLifeTimeTestJob", SFreeFunctionLifeTimeTestJob, SFreeFunctionLifeTimeTestCallback);

TEST_F(CJobSystemTest, FreeFunctionJobLifeTime)
{
	gFreeFunctionLifetimeTestStringResult = string();

	auto GetLifeTimeTestHostJob = []
	{
		string str = "abc";
		SFreeFunctionLifeTimeTestJob* pJob = new SFreeFunctionLifeTimeTestJob(str);
		return pJob;
	};

	SFreeFunctionLifeTimeTestJob* pJob = GetLifeTimeTestHostJob();
	JobManager::SJobState jobState;
	pJob->RegisterJobState(&jobState);
	pJob->Run();
	jobState.Wait();
	REQUIRE(gFreeFunctionLifetimeTestStringResult == "abc");
	delete pJob;
}

TEST_F(CJobSystemTest, MoveConstructor)
{
	CTestJobHost host;
	TTestJob job(42);
	job.SetClassInstance(&host);
	job.SetPriorityLevel(JobManager::eStreamPriority);
	TTestJob job2 = std::move(job);
	job2.Run();
	while (!host.IsDoneCalculating()) {}
	REQUIRE(host.GetValue() == 42);
}

TEST_F(CJobSystemTest, LambdaJobConcise)
{
	std::atomic<int> v{ 0 };
	JobManager::SJobState jobState;
	for (int i = 0; i < 100; i++)
	{
		gEnv->pJobManager->AddLambdaJob("ExampleJob1", [&v]
		{
			++v;
		}, JobManager::eRegularPriority, &jobState);
	}
	jobState.Wait();
	REQUIRE(static_cast<int>(v) == 100);
}


DECLARE_LAMBDA_JOB("TestLambdaJob", TTestLambdaJob);
DECLARE_LAMBDA_JOB("TestLambdaJob2", TTestLambdaJob2, void(int));

struct SDestructorDetector
{
	static bool isCalled;

	SDestructorDetector() = default;
	SDestructorDetector(const SDestructorDetector&) = default;
	SDestructorDetector(SDestructorDetector&&) = default;
	SDestructorDetector& operator=(const SDestructorDetector&) = default;
	SDestructorDetector& operator=(SDestructorDetector&&) = default;

	~SDestructorDetector()
	{
		isCalled = true;
	}
};

bool SDestructorDetector::isCalled = false;

TEST_F(CJobSystemTest, LambdaJobVerbose)
{
	int v = 0;

	SDestructorDetector destructorDetector;
	JobManager::SJobState jobState;

	// We test that the lambda is properly called and the lambda and all the captures are destructed.
	{
		TTestLambdaJob job = [&v, destructorDetector]
		{
			v = 20;
		};
		job.SetPriorityLevel(JobManager::eRegularPriority);
		job.RegisterJobState(&jobState);
		SDestructorDetector::isCalled = false;
		job.Run();
		REQUIRE(!SDestructorDetector::isCalled);
		jobState.Wait();
		REQUIRE(SDestructorDetector::isCalled);
		REQUIRE(v == 20);
	}

	{
		TTestLambdaJob2 job([&v](int x)
		{
			v = x;
		}, 23);
		job.SetPriorityLevel(JobManager::eRegularPriority);
		job.RegisterJobState(&jobState);
		job.Run();
		jobState.Wait();
		REQUIRE(v == 23);
	}
}

TEST_F(CJobSystemTest, PostJob)
{
	JobManager::SJobState jobState;
	std::atomic<int> x{ 0 };

	DECLARE_LAMBDA_JOB("ExampleJob2", TExampleJob2);
	TExampleJob2 job1 = [&]
	{
		REQUIRE(static_cast<int>(x) == 0);
		x++;
	};
	DECLARE_LAMBDA_JOB("PostJob", TPostJob);
	TPostJob postJob = [&]
	{
		REQUIRE(static_cast<int>(x) == 1);
		x++;
	}; 
	postJob.RegisterJobState(&jobState);
	jobState.RegisterPostJob(std::move(postJob));

	job1.Run(JobManager::eRegularPriority, &jobState);

	jobState.Wait();
	REQUIRE(static_cast<int>(x) == 2);
}

TEST_F(CJobSystemTest, JobState)
{
	JobManager::SJobState jobState;
	JobManager::SJobState jobState2 = jobState;
	jobState.SetRunning();
	REQUIRE(jobState2.IsRunning());
	jobState2.SetStopped();
	REQUIRE(!jobState.IsRunning());
}

#if !defined(_RELEASE) //Job system filtering is a debugging feature and not part of the release build
TEST_F(CJobSystemTest, Filter)
{
	JobManager::SJobState jobState;
	JobManager::SJobState jobState2;

	std::thread::id threadId = std::this_thread::get_id();
	std::thread::id jobThreadId1, jobThreadId2;

	gEnv->pJobManager->SetJobFilter("Job1");

	gEnv->pJobManager->AddLambdaJob("Job1", [&]
	{
		jobThreadId1 = std::this_thread::get_id();
	}, JobManager::eRegularPriority, &jobState);

	gEnv->pJobManager->AddLambdaJob("Job2", [&]
	{
		jobThreadId2 = std::this_thread::get_id();
	}, JobManager::eRegularPriority, &jobState2);

	jobState.Wait();
	jobState2.Wait();

	REQUIRE(jobThreadId1 == threadId);
	REQUIRE(jobThreadId2 != threadId);
}
#endif

TEST_F(CJobSystemTest, DisableJobSystem)
{
	gEnv->pJobManager->SetJobSystemEnabled(0);

	JobManager::SJobState jobState;
	std::thread::id threadId = std::this_thread::get_id();
	std::thread::id jobThreadId;
	REQUIRE(jobThreadId != threadId);
	gEnv->pJobManager->AddLambdaJob("Job", [&]
	{
		jobThreadId = std::this_thread::get_id();
	}, JobManager::eRegularPriority, &jobState);
	REQUIRE(jobThreadId == threadId);
	jobState.Wait();
	gEnv->pJobManager->SetJobSystemEnabled(1);
}
