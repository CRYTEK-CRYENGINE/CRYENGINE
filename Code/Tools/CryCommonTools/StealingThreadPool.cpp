// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "StealingThreadPool.h"
#include "ResourceCompiler.h"

namespace ThreadUtils {

class StealingWorker
{
public:
	StealingWorker(StealingThreadPool* pool, int index, bool trace, ConditionVariable &jobsCV)
	: m_pool(pool)
	, m_index(index)
	, m_handle(0)
	, m_tracingEnabled(trace)
	, m_lastStartTime(0)
	, m_exitFlag(0)	
	, m_jobsCV(jobsCV)
	{
	}

	static unsigned int __stdcall ThreadFunc(void* param)
	{
		StealingWorker* self = static_cast<StealingWorker*>(param);
		self->Work();
		return 0;
	}

	void Start(int startTime)
	{
		m_lastStartTime = startTime;

		m_handle = (HANDLE)_beginthreadex(0, 0, &StealingWorker::ThreadFunc, (void*)this, 0, 0);

		string threadName;
		threadName.Format("StealingWorker %d", m_index);
		ThreadUtils::SetThreadName(GetThreadId(m_handle), threadName.c_str());
	}

	bool GetJobLockless(Job& job)
	{
		if (m_jobs.empty())
			return false;
		job = m_jobs.front();
		m_jobs.pop_front();
		
		return true;
	}

	bool GetJob(Job& job)
	{
		AutoLock lock(m_lockJobs);
		return GetJobLockless(job);
	}

	void ExecuteJob(Job& job)
	{
		InterlockedDecrement(&m_pool->m_numJobsWaitingForExecution);

		job.Run();

		if (m_tracingEnabled)
		{
			int time = (int)GetTickCount();

			JobTrace trace;
			trace.m_job = job;
			trace.m_duration = time - m_lastStartTime;
			m_traces.push_back(trace);

			m_lastStartTime = time;
		}
				
		InterlockedDecrement(&m_pool->m_numJobs);
		m_pool->m_jobFinishedCV.WakeAll();
	}

	bool TryToStealJob(Job& job)
	{
		while (true)
		{
			StealingWorker* victim = m_pool->FindBestVictim(m_index);
			if (!victim)
			{
				return false;
			}
			if (StealJobs(job, victim))
			{
				return true;
			}
		}
	}

	void Work()
	{
		Job job;

		while (true)
		{
			CriticalSection cs;

			while (m_pool->m_numJobsWaitingForExecution == 0)
			{
				m_jobsCV.Sleep(cs);

				if (m_exitFlag == 1)
				{
					return;
				}
			}
			
			if (GetJob(job))
			{
				ExecuteJob(job);
			}
			else if (TryToStealJob(job))
			{
				ExecuteJob(job);
			}
		}
	}

	// Called from different worker thread
	bool StealJobs(Job& job, StealingWorker* victim)
	{
		if (victim == this)
		{
			assert(0 && "Trying to steal own jobs");
			return false;
		}

		bool order = m_index < victim->m_index;
		AutoLock lock1(order ? m_lockJobs : victim->m_lockJobs);
		AutoLock lock2(order ? victim->m_lockJobs : m_lockJobs);

		if (victim->m_jobs.empty())
		{
			return false;
		}

		int numJobs = (int)victim->m_jobs.size();
		size_t stealUntil = numJobs - numJobs / 2;
		Jobs::iterator begin = victim->m_jobs.begin(); 
		Jobs::iterator end = victim->m_jobs.begin() + stealUntil;

		m_jobs.insert(m_jobs.end(), begin, end);
		victim->m_jobs.erase(begin, end);

		return GetJobLockless(job);
	}

	// Called from any thread
	void Submit(const Job& job)
	{
		AutoLock lock(m_lockJobs);

		m_jobs.push_back(job);
		m_jobs.back().m_debugInitialThread = m_index;		

		m_jobsCV.Wake();
	}

	// Called from any thread
	void Submit(const Jobs& jobs)
	{
		const size_t numJobs = jobs.size();

		AutoLock lock(m_lockJobs);
				
		m_jobs.insert(m_jobs.begin(), jobs.begin(), jobs.end());		
		for(size_t i = 0; i < numJobs; ++i)
		{
			m_jobs[i].m_debugInitialThread = m_index;
		}

		m_jobsCV.Wake();
	}

	long NumJobsPending() const 
	{ 
		AutoLock lock(m_lockJobs);
		return m_jobs.size();
	}

	// Called from main thread
	void SignalExit()
	{
		InterlockedCompareExchange(&m_exitFlag, 1, 0);		
	}

	void GetTraces(JobTraces& traces)
	{
		if (m_tracingEnabled)
		{
			m_traces.swap(traces);
		}
	}

private:
	StealingThreadPool* m_pool;
	HANDLE m_handle;
	int m_index;
	bool m_tracingEnabled;
	int m_lastStartTime;
	JobTraces m_traces;

	Jobs m_jobs;
	mutable CriticalSection m_lockJobs;
	ConditionVariable &m_jobsCV;
	
	LONG m_exitFlag;
	friend class StealingThreadPool;
};

// ---------------------------------------------------------------------------

StealingThreadPool::StealingThreadPool(int numThreads, bool enableTracing)
: m_numThreads(numThreads)
, m_numJobs(0)
, m_numJobsWaitingForExecution(0)
, m_enableTracing(enableTracing)
{
	m_workers.resize(numThreads);
	for (int i = 0; i < numThreads; ++i)
	{
		m_workers[i] = new StealingWorker(this, i, m_enableTracing, m_jobsCV);
	}
}

StealingThreadPool::~StealingThreadPool()
{
	WaitAllJobs();

	size_t numThreads = m_workers.size();
	for (size_t i = 0; i < numThreads; ++i)
	{		
		m_workers[i]->SignalExit();
	}
	m_jobsCV.WakeAll();	

	m_threadTraces.resize(numThreads);
	for (size_t i = 0; i < numThreads; ++i)
	{		
		m_workers[i]->GetTraces(m_threadTraces[i]);
	}
}

void StealingThreadPool::Start()
{
	int startTime = (int)GetTickCount();
	size_t numThreads = m_workers.size();
	for (int i = 0; i < numThreads; ++i)
	{
		m_workers[i]->Start(startTime);
	}
}

void StealingThreadPool::WaitAllJobs()
{
	CriticalSection cs;
	while (m_numJobs > 0)
	{
	 	m_jobFinishedCV.Sleep(cs);
	}
}

void StealingThreadPool::WaitAllJobsTemporary()
{
	CriticalSection cs;
	while (m_numJobs > 0)
	{
//		m_jobFinishedCV.Sleep(cs);
	}
}

// Called from any thread
void StealingThreadPool::Submit(const Job& job)
{
	InterlockedIncrement(&m_numJobs);
	InterlockedIncrement(&m_numJobsWaitingForExecution);

	if (StealingWorker* worker = FindWorstWorker())
	{
		worker->Submit(job);
	}
}

// Called from any thread
void StealingThreadPool::Submit(const Jobs& jobs)
{
	InterlockedExchangeAdd(&m_numJobs, static_cast<LONG>(jobs.size()));
	InterlockedExchangeAdd(&m_numJobsWaitingForExecution, static_cast<LONG>(jobs.size()));

	if (StealingWorker* worker = FindWorstWorker())
	{
		worker->Submit(jobs);
	}
}

JobGroup* StealingThreadPool::CreateJobGroup(JobFunc func, void* data)
{
	return new JobGroup(this, func, data);
}

StealingWorker* StealingThreadPool::FindBestVictim(int exceptFor) const
{
	int maxJobs = 0;
	StealingWorker* bestVictim = 0;
	for (size_t i = 0; i < m_workers.size(); ++i)
	{
		if (i == exceptFor)
			continue;
		StealingWorker* worker = m_workers[i];
		long numJobs = worker->NumJobsPending();
		if (numJobs > maxJobs)
		{
			maxJobs = numJobs;
			bestVictim = worker;
		}
	}
	return bestVictim;
}

StealingWorker* StealingThreadPool::FindWorstWorker() const
{
	if (m_workers.empty())
		return 0;

	int minJobs = INT_MAX;
	StealingWorker* worstWorker = m_workers[0];
	for (size_t i = 0; i < m_workers.size(); ++i)
	{
		StealingWorker* worker = m_workers[i];
		long numJobs = worker->NumJobsPending();
		if (numJobs < minJobs)
		{
			minJobs = numJobs;
			worstWorker = worker;
		}
	}
	return worstWorker;
}

static bool WriteString( FILE* f, const char* str )
{
	return fwrite(str, strlen(str), 1, f) == 1;
}

static int Interpolate( int a, int b, float phase )
{
	return int(float(a) + float(b - a) * phase);
}

static int InterpolateColor( int c1, int c2, float phase )
{
	const int r1 = (c1 & 0x0000ff);
	const int g1 = (c1 & 0x00ff00) >> 8;
	const int b1 = (c1 & 0xff0000) >> 16;
	const int r2 = (c2 & 0x0000ff);
	const int g2 = (c2 & 0x00ff00) >> 8;
	const int b2 = (c2 & 0xff0000) >> 16;

	const int r = min(255, max(0, Interpolate(r1, r2, phase)));
	const int g = min(255, max(0, Interpolate(g1, g2, phase)));
	const int b = min(255, max(0, Interpolate(b1, b2, phase)));

	return r + (g << 8) + (b << 16);
}

static const int g_animColors[] = {
	0xff0000, 0x0000ff, 0x00ff00,
	0xffff00, 0xff00ff, 0x00ffff,
	0xff8080, 0x8080ff, 0x80ff80,
	0xffff80, 0xff80ff, 0x80ffff
};

static int ColorizeJobTrace(const ThreadUtils::JobTrace& trace)
{
	const int numColors = sizeof(g_animColors) / sizeof(g_animColors[0]);
	const int initialThread = trace.m_job.m_debugInitialThread;
	const int index = initialThread % numColors;
	const float brightness = pow(0.5f, initialThread / numColors);
	return InterpolateColor(0, InterpolateColor(g_animColors[index], 0xffffff, 0.5f), brightness);
}

bool StealingThreadPool::SaveTracesGraph(const char* filename)
{
	if (!m_enableTracing)
	{
		return false;
	}

	const float screenWidth = 1240.0f;

	float duration = 0;
	for (size_t t = 0; t < m_threadTraces.size(); ++t)
	{
		float threadDuration = 0;
		const JobTraces& traces = m_threadTraces[t];
		for (int i = 0; i < traces.size(); ++i)
		{
			threadDuration += traces[i].m_duration;
		}
		duration = max(threadDuration, duration);
	}

	const float padding = 10.0f;
	const float rowHeight = 60.0f;
	const float xScale = fabsf(duration) > FLT_EPSILON ? (screenWidth - padding * 2.0f) / duration : 1.0f;

	const float width = screenWidth;
	const float height = (m_threadTraces.size() + 0.5f) * rowHeight;

	FILE* f = fopen(filename, "wt");
	if (!f)
	{
		return false;
	}

	char buf[4096];
	_snprintf_s(buf, sizeof(buf), _TRUNCATE,
							 "<?xml version='1.0' encoding='UTF-8' standalone='no'?>\n"
							 "<svg\n"
							 "   xmlns:dc='http://purl.org/dc/elements/1.1/'\n"
							 "   xmlns:cc='http://creativecommons.org/ns#'\n"
							 "   xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'\n"
							 "   xmlns:svg='http://www.w3.org/2000/svg'\n"
							 "   xmlns='http://www.w3.org/2000/svg'\n"
							 "   xmlns:sodipodi='http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd'\n"
							 "   xmlns:inkscape='http://www.inkscape.org/namespaces/inkscape'\n"
							 "   width='%f'\n"
							 "   height='%f'\n"
							 "   id='svg2'\n"
							 "   version='1.1'\n"
							 "	 >\n",
							 width, height 
							 );

	if (!WriteString( f, buf ))
	{
		return false;
	}

	for (size_t t = 0; t < m_threadTraces.size(); ++t)
	{
		float x = padding;
		float y = rowHeight * 0.5f + rowHeight * t;

		_snprintf_s(buf, sizeof(buf), _TRUNCATE,
							 "    <text\n"
							 "       xml:space='preserve'\n"
							 "       style='font-size:40px;font-style:normal;font-weight:normal;line-height:125%%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:none;font-family:Sans'\n"
							 "       x='%f'\n"
							 "       y='%f'\n"
							 "       sodipodi:linespacing='125%%'><tspan sodipodi:role='line' x='%f' y='%f' style='font-size:12px;fill:#000000'>Thread %" PRISIZE_T "</tspan></text>\n",
							 x, y, x, y, t + 1);

		if (!WriteString( f, buf ))
		{
			return false;
		}


		y += padding;

		const ThreadUtils::JobTraces& traces = m_threadTraces[t];
		for (int i = 0; i < traces.size(); ++i)
		{
			const float width = traces[i].m_duration * xScale;
			const float height = rowHeight * 0.5f;

			const int color = ColorizeJobTrace(traces[i]);
			const int strokeColor = 0;
			_snprintf_s(buf, sizeof(buf), _TRUNCATE,
								 "    <rect\n"
								 "       style='fill:#%06x;fill-rule:evenodd;stroke:#%06x;stroke-width:0.25px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1'\n"
								 "       width='%f'\n"
								 "       height='%f'\n"
								 "       x='%f'\n"
								 "       y='%f' />\n",
								 color, strokeColor, width, height, x, y);

			if (!WriteString(f, buf))
			{
				return false;
			}

			x += width;
		}

		y += rowHeight;
	}

	if (!WriteString( f, "\n</svg>\n" ))
	{
		return false;
	}

	fclose( f );
	return true;
}

// ---------------------------------------------------------------------------

void JobGroup::Process(JobGroup::GroupInfo* info)
{
	info->m_job.Run();

	long jobsLeft = InterlockedDecrement(&info->m_group->m_numJobsRunning);
	assert(jobsLeft >= 0);
	if (jobsLeft == 0)
	{
		info->m_group->m_finishJob.Run();
		delete info->m_group;
	}
}

JobGroup::JobGroup(StealingThreadPool* pool, JobFunc func, void* data)
: m_pool(pool)
, m_numJobsRunning(0)
, m_finishJob(func, data)
, m_submited(false)
{
}

void JobGroup::Submit()
{
	if (m_submited)
	{
		assert(0);
		return;
	}

	if (m_numJobsRunning == 0)
	{
		m_pool->Submit(m_finishJob);
		return;
	}
	
	Jobs jobs;
	jobs.resize(m_infos.size());
	for (size_t i = 0; i < m_infos.size(); ++i)
	{
		jobs[i] = Job((JobFunc)&JobGroup::Process, &m_infos[i]);
	}

	m_pool->Submit(jobs);
}

void JobGroup::Add(JobFunc func, void* data)
{
	if (m_submited)
	{
		assert(0);
		return;
	}

	GroupInfo info;
	info.m_job = Job(func, data);
	info.m_group = this;

	m_infos.push_back(info);
	++m_numJobsRunning;
}

}
