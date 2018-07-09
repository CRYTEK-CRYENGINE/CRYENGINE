// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//===========================================================================

#ifndef ThreadSamplerH
#define ThreadSamplerH

#include "ThreadProfiler.h"
#include <CryThreading/IThreadManager.h>

#if defined(THREAD_SAMPLER)
	#define WIN_THREAD_SAMPLER
#endif

#if defined(WIN_THREAD_SAMPLER)

	#define TIMERSAMPLER

	#include <CryCore/Platform/CryWindows.h>
	#include <vector>
	#include <map>

	#pragma pack(1)
//========================================================
// class CWinThreadSampler
//========================================================
class CWinThreadSampler : public IThreadSampler
{
public:

	//number of samles in driver cyclic buffer
	#ifdef TIMERSAMPLER
	static const int MAX_CPU_COUNT = 4;
	static const int CPU_SAMPLES_COUNT = 4000;
	static const int DRIVERSAMPLESCOUNT = MAX_CPU_COUNT * CPU_SAMPLES_COUNT;
	#else
	static const int DRIVERSAMPLESCOUNT = 10000;
	#endif

	//dummy ThreadId for CreateSpanListForThread() method
	static const DWORD OTHER_THREADS = MAXDWORD;

	//we will request snapshot from driver once per 2 seconds
	static const DWORD SNAPSHOT_UPDATE_PERIOD = 2000;

	//======================
	//TSample
	//======================
	//sample from driver
	typedef struct
	{
	#ifdef TIMERSAMPLER
		DWORD   processId;
		DWORD   threadId;
	#else
		DWORD   fromProcessId;
		DWORD   fromThreadId;
		DWORD   toProcessId;
		DWORD   toThreadId;
	#endif
		__int64 RDTSC;
	} TSample;

	struct SnapshotInfo
	{
		// Pointer to the array of context switches, one int per processor, size of the array is given in nProcessorCount.
		int pContextSwitches[1];
		int nProcessorCount;
	};

	//======================
	//class TSamples
	//======================
	//class for holding snapshot of samples buffer, recevied from driver
	//CBuilder does not handle <int n> template well (wrong sizeof()),
	//se we have to use macro
	#define DEFINE_TSAMPLES(n, m)                     \
	  class TSamples ## m                             \
	  {                                               \
	  public:                                         \
	    DWORD size() const                            \
	    {                                             \
	      return n;                                   \
	    }                                             \
	                                                  \
	    DWORD memorySize() const                      \
	    {                                             \
	      return n * sizeof(TSample) + sizeof(DWORD); \
	    }                                             \
	                                                  \
	    TSample& operator[](DWORD index)              \
	    {                                             \
	      index = (index + driverData.index) % n;     \
	      return driverData.samples[index];           \
	    }                                             \
	                                                  \
	    friend class CWinThreadSampler;               \
	                                                  \
	  private:                                        \
	    TSamples ## m()                               \
	    {                                             \
	      memset(&driverData, 0, sizeof(driverData)); \
	    };                                            \
	                                                  \
	    struct                                        \
	    {                                             \
	      DWORD   index;                              \
	      TSample samples[n];                         \
	    } driverData;                                 \
	  };

	DEFINE_TSAMPLES(DRIVERSAMPLESCOUNT, ALL)

	#ifdef TIMERSAMPLER
	DEFINE_TSAMPLES(CPU_SAMPLES_COUNT, CPU)
	#endif

	typedef SThreadDisplaySpan TSpan;

	//======================
	//TProcessItem
	//======================
	typedef struct
	{
		DWORD processId;
		char  exeName[MAX_PATH];
	} TProcessItem;

	typedef std::vector<DWORD>        TThreadsList;
	typedef std::vector<TProcessItem> TProcessesList;

	//Processes snapshot is built with EnumerateProcesses();
	TProcessesList processes;

	//Threads snapshot is built with EnumerateThreads();
	TThreadsList threads;

	//samples from driver
	TSamplesALL samples;

	CWinThreadSampler();
	~CWinThreadSampler();

	enum { NUM_HW_THREADS = 1 };

	// IThreadSampler interface
	virtual void        EnumerateThreads(TProcessID processId);
	virtual int         GetNumHWThreads()                  { return NUM_HW_THREADS; }
	virtual int         GetNumThreads()                    { return threads.size(); }
	virtual threadID    GetThreadId(int idx)               { return threads[idx]; }
	virtual const char* GetThreadName(threadID threadId);
	virtual float       GetExecutionTimeFrame()            { return 0.0f; }
	virtual float       GetExecutionTime(ETraceThreads tt) { return 0.0f; }

	// Update samples snapshot from driver
	// should be called at least once per two seconds
	virtual void                 Tick();
	virtual const SSnapshotInfo* GetSnapshot() { return &m_snapshotInfo; }

	virtual void                 CreateSpanListForThread(TProcessID processId, threadID threadId,
	                                                     TTDSpanList& spanList,
	                                                     uint32 width, uint32 scale,
	                                                     uint32* totalTime, int* processorId, uint32* color);
	// ~IThreadSampler interface

	//get current Time Stamp Counter value from CPU1
	static __int64 RDTSC();

	static void    SetThreadName(const char* name);
	static void    SetThreadNameEx(DWORD threadId, const char* name);
	//can return "Unknown"

	void EnumerateProcesses();

	__int64 RDTSCperSecond;

private:
	typedef std::map<DWORD, char*> TThreadNames;
	static TThreadNames threadNames;

	DWORD               lastSnapshot;
	SSnapshotInfo       m_snapshotInfo;

	__int64             lastRDTSC;
	__int64             lastPC;

	void LoadDriver();
	void UnloadDriver();
	void GetDriverFileName(char* fnme);
	#ifdef TIMERSAMPLER
	void MergeCPUData(TSamplesCPU* CPUData);
	#endif

};
	#pragma pack()

#endif  // defined(WIN_THREAD_SAMPLER)

#endif  // ThreadSamplerH
